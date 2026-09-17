#ifndef BUTTON_GESTURES_H
#define BUTTON_GESTURES_H

#include <stdbool.h>
#include <stdint.h>

#define DOUBLE_CLICK_MS 350
#define LONG_PRESS_MS 800

enum button_event {
	BUTTON_PRESS = 0x01,
	BUTTON_DOUBLE_PRESS = 0x02,
	BUTTON_LONG_RELEASE = 0x04, /* BTHome long_press: release after hold */
	BUTTON_HOLD = 0x80,         /* BTHome hold_press: threshold crossed */
};

struct button_gestures {
	bool down;
	bool held;
	bool waiting_second;
	bool second_down;
	int64_t pressed_at;
	int64_t click_deadline;
};

typedef void (*button_emit_fn)(uint8_t event, void *context);
void button_gestures_tick(struct button_gestures *s, int64_t now,
			  button_emit_fn emit, void *context);
void button_gestures_edge(struct button_gestures *s, bool down, int64_t now,
			  button_emit_fn emit, void *context);
int64_t button_gestures_deadline(const struct button_gestures *s);

#endif
