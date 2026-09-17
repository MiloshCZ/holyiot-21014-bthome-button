#include "button_gestures.h"

void button_gestures_tick(struct button_gestures *s, int64_t now,
			  button_emit_fn emit, void *context)
{
	if (s->down && !s->held && now >= s->pressed_at + LONG_PRESS_MS) {
		s->held = true;
		s->second_down = false;
		emit(BUTTON_HOLD, context);
	}
	if (s->waiting_second && now >= s->click_deadline) {
		s->waiting_second = false;
		emit(BUTTON_PRESS, context);
	}
}

void button_gestures_edge(struct button_gestures *s, bool down, int64_t now,
			  button_emit_fn emit, void *context)
{
	/* Resolve expired deadlines before the edge, including exact boundaries. */
	button_gestures_tick(s, now, emit, context);
	if (down == s->down) {
		return;
	}
	s->down = down;
	if (down) {
		s->pressed_at = now;
		s->held = false;
		s->second_down = s->waiting_second;
		s->waiting_second = false;
		return;
	}
	if (s->held) {
		emit(BUTTON_LONG_RELEASE, context);
	} else if (s->second_down) {
		emit(BUTTON_DOUBLE_PRESS, context);
	} else {
		s->waiting_second = true;
		s->click_deadline = now + DOUBLE_CLICK_MS;
	}
	s->held = false;
	s->second_down = false;
}

int64_t button_gestures_deadline(const struct button_gestures *s)
{
	if (s->down && !s->held) {
		return s->pressed_at + LONG_PRESS_MS;
	}
	return s->waiting_second ? s->click_deadline : INT64_MAX;
}
