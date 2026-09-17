#include <zephyr/drivers/adc.h>
#include <hal/nrf_saadc.h>
#include "battery.h"

static const struct device *const adc = DEVICE_DT_GET(DT_NODELABEL(adc));
static const struct adc_channel_cfg channel = {
	.gain = ADC_GAIN_1_6,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10),
	.channel_id = 0,
	.input_positive = NRF_SAADC_INPUT_VDD,
};
static int16_t sample __aligned(4);
static bool configured;
static bool calibrated;

int battery_read_mv(uint16_t *millivolts, bool recalibrate)
{
	if (!device_is_ready(adc)) { return -ENODEV; }
	if (!configured) {
		int err = adc_channel_setup(adc, &channel);
		if (err) { return err; }
		configured = true;
	}
	const struct adc_sequence sequence = {
		.channels = BIT(0),
		.buffer = &sample,
		.buffer_size = sizeof(sample),
		.resolution = 12,
		.oversampling = 3, /* Eight hardware samples, one short measurement. */
		.calibrate = !calibrated || recalibrate,
	};
	/* Zephyr's nrfx simple-mode driver disables SAADC on completion.
	 * No polling, periodic ADC timer, external divider or persistent HF clock. */
	int err = adc_read(adc, &sequence);
	if (err) {
		calibrated = false;
		return err;
	}
	calibrated = true;
	int32_t mv = sample;
	err = adc_raw_to_millivolts(adc_ref_internal(adc), channel.gain, 12, &mv);
	if (err) { return err; }
	if (mv < 1500 || mv > 3600) { return -ERANGE; }
	*millivolts = (uint16_t)mv;
	return 0;
}
