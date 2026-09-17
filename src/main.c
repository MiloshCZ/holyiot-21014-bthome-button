#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/util.h>
#include "button_gestures.h"
#include "app_config.h"
#include "battery.h"
#include "bthome_packet.h"

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static struct gpio_callback button_callback;
static struct k_work_delayable debounce_work;
struct button_edge {
	int64_t time;
	bool down;
};
K_MSGQ_DEFINE(edge_queue, sizeof(struct button_edge), 16, 8);
K_MSGQ_DEFINE(event_queue, sizeof(uint8_t), 16, 1);
static bool button_down;
static struct button_gestures gestures;
/* Exposed for SWD inspection and accelerated heartbeat verification. */
volatile int64_t heartbeat_at;

/* Readable through SWD without a serial console. */
volatile struct {
	uint32_t ready;
	uint32_t presses;
	uint32_t bursts;
	int32_t error;
	uint32_t singles;
	uint32_t doubles;
	uint32_t holds;
	uint32_t releases;
	uint32_t last_event;
	uint32_t battery_mv;
	uint32_t battery_samples;
	int32_t battery_error;
	uint32_t heartbeats;
} diagnostics;

/* BTHome v2, unencrypted, trigger-based; packet ID; button event.
 * A repeated advertisement retains its packet ID to suppress duplicates. */
static uint8_t packet_id;
static uint8_t service_data[BTHOME_PAYLOAD_MAX];
BUILD_ASSERT(3 + 2 + sizeof(CONFIG_BT_DEVICE_NAME) - 1 + 2 + BTHOME_PAYLOAD_MAX <= 31,
	     "Legacy BLE advertisement exceeds 31 bytes");
static struct bt_data advertising_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA(BT_DATA_SVC_DATA16, service_data, sizeof(service_data)),
};
static const struct bt_le_adv_param advertising_params = {
	.id = BT_ID_DEFAULT,
	.options = BT_LE_ADV_OPT_USE_IDENTITY,
	.interval_min = 160, /* 100 ms */
	.interval_max = 192, /* 120 ms */
};

static void fatal(int error)
{
	diagnostics.error = error;
	(void)bt_le_adv_stop();
	(void)gpio_pin_set_dt(&green, 0);
	(void)gpio_pin_set_dt(&blue, 0);
	for (;;) {
		(void)gpio_pin_set_dt(&red, 1);
		k_sleep(K_MSEC(20));
		(void)gpio_pin_set_dt(&red, 0);
		k_sleep(K_SECONDS(5));
	}
}

static void prepare_report(uint8_t event, bool recalibrate)
{
	uint16_t mv = 0;
	int err = battery_read_mv(&mv, recalibrate);
	diagnostics.battery_error = err;
	if (!err) {
		diagnostics.battery_mv = mv;
		++diagnostics.battery_samples;
	}
	/* ADC failure must not disable the button or broadcast a made-up voltage. */
	advertising_data[2].data_len =
		bthome_encode(service_data, packet_id, event, mv, err == 0);
}

static void debounce(struct k_work *work)
{
	ARG_UNUSED(work);
	int state = gpio_pin_get_dt(&button);
	if (state < 0) {
		diagnostics.error = state;
		return;
	}
	if ((state != 0) != button_down) {
		struct button_edge edge = {.time = k_uptime_get(), .down = state != 0};
		if (k_msgq_put(&edge_queue, &edge, K_NO_WAIT) != 0) {
			diagnostics.error = -ENOSPC;
		}
	}
	button_down = state != 0;
}

static void queue_event(uint8_t event, void *context)
{
	ARG_UNUSED(context);
	if (k_msgq_put(&event_queue, &event, K_NO_WAIT) != 0) {
		fatal(-ENOSPC);
	}
	switch (event) {
	case BUTTON_PRESS: ++diagnostics.singles; break;
	case BUTTON_DOUBLE_PRESS: ++diagnostics.doubles; break;
	case BUTTON_HOLD: ++diagnostics.holds; break;
	case BUTTON_LONG_RELEASE: ++diagnostics.releases; break;
	}
}

static void button_changed(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_work_reschedule(&debounce_work, K_MSEC(DEBOUNCE_MS));
}

int main(void)
{
	const struct gpio_dt_spec *leds[] = {&red, &green, &blue};
	int err;
	if (!gpio_is_ready_dt(&button)) { fatal(-ENODEV); }
	for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
		if (!gpio_is_ready_dt(leds[i])) { fatal(-ENODEV); }
		err = gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_INACTIVE);
		if (err) { fatal(err); }
	}
	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err) { fatal(err); }
	k_work_init_delayable(&debounce_work, debounce);
	gpio_init_callback(&button_callback, button_changed, BIT(button.pin));
	err = gpio_add_callback(button.port, &button_callback);
	if (err) { fatal(err); }
	err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (err) { fatal(err); }
	k_work_reschedule(&debounce_work, K_MSEC(DEBOUNCE_MS));
	err = bt_enable(NULL);
	if (err) { fatal(err); }
	prepare_report(0, true);
	err = bt_le_adv_start(&advertising_params, advertising_data,
		ARRAY_SIZE(advertising_data), NULL, 0);
	if (err) { fatal(err); }
	diagnostics.ready = 1;
	diagnostics.bursts = 1;
	bool advertising = true;
	int64_t stop_at = k_uptime_get() + DISCOVERY_MS;
	int64_t replace_at = 0; /* Startup discovery may be replaced immediately. */
	heartbeat_at = k_uptime_get() + HEARTBEAT_INTERVAL_MS;
	int64_t led_off_at = k_uptime_get() + LED_MS;
	gpio_pin_set_dt(&blue, 1);

	for (;;) {
		int64_t now = k_uptime_get();
		if (led_off_at && now >= led_off_at) {
			gpio_pin_set_dt(&green, 0);
			gpio_pin_set_dt(&blue, 0);
			led_off_at = 0;
		}
		if (advertising && now >= stop_at) {
			err = bt_le_adv_stop();
			if (err) { fatal(err); }
			advertising = false;
		}
		int64_t next = advertising ? stop_at : INT64_MAX;
		if (led_off_at && led_off_at < next) { next = led_off_at; }
		int64_t gesture_at = button_gestures_deadline(&gestures);
		if (gesture_at < next) { next = gesture_at; }
		bool queued = k_msgq_num_used_get(&event_queue) != 0;
		if (queued && replace_at < next) { next = replace_at; }
		/* Let an in-progress gesture finish first. Every event also reports
		 * voltage, so it postpones the idle heartbeat instead of adding traffic. */
		bool gesture_pending = gestures.down || gestures.waiting_second;
		if (!gesture_pending && !queued) {
			int64_t heartbeat_wakeup = MAX(heartbeat_at, replace_at);
			if (heartbeat_wakeup < next) { next = heartbeat_wakeup; }
		}
		k_timeout_t timeout = next == INT64_MAX ? K_FOREVER : K_MSEC(MAX(next - now, 0));
		struct button_edge edge;
		if (k_msgq_get(&edge_queue, &edge, timeout) == 0) {
			if (edge.down) { ++diagnostics.presses; }
			button_gestures_edge(&gestures, edge.down, edge.time, queue_event, NULL);
		} else {
			button_gestures_tick(&gestures, k_uptime_get(), queue_event, NULL);
		}
		now = k_uptime_get();
		if (advertising && now < replace_at) { continue; }
		uint8_t event;
		bool is_heartbeat = false;
		if (k_msgq_get(&event_queue, &event, K_NO_WAIT) != 0) {
			if (now < heartbeat_at || gestures.down || gestures.waiting_second) { continue; }
			event = 0;
			is_heartbeat = true;
		}
		++packet_id;
		prepare_report(event, is_heartbeat);
		if (advertising) {
			err = bt_le_adv_update_data(advertising_data, ARRAY_SIZE(advertising_data), NULL, 0);
		} else {
			err = bt_le_adv_start(&advertising_params, advertising_data,
				ARRAY_SIZE(advertising_data), NULL, 0);
		}
		if (err) { fatal(err); }
		advertising = true;
		stop_at = k_uptime_get() +
			(is_heartbeat ? HEARTBEAT_ADVERTISING_MS : EVENT_ADVERTISING_MS);
		replace_at = k_uptime_get() + MIN_EVENT_AIRTIME_MS;
		heartbeat_at = k_uptime_get() + HEARTBEAT_INTERVAL_MS;
		if (is_heartbeat) {
			++diagnostics.heartbeats;
		} else {
			led_off_at = k_uptime_get() + LED_MS;
			gpio_pin_set_dt(&green, 1);
		}
		diagnostics.last_event = event;
		++diagnostics.bursts;
	}
}
