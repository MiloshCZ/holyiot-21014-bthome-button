#include "button_gestures.h"

static uint8_t events[16];
static unsigned count;
static struct button_gestures state;
unsigned tests_passed;
int run_packet_tests(void);

static void emit(uint8_t event, void *context)
{
	(void)context;
	if (count < sizeof(events)) { events[count] = event; }
	++count;
}

static void reset(void)
{
	state.down = state.held = state.waiting_second = state.second_down = false;
	state.pressed_at = state.click_deadline = 0;
	count = 0;
}

#define CHECK(c) do { if (!(c)) { return __LINE__; } } while (0)
#define EDGE(d, t) button_gestures_edge(&state, d, t, emit, 0)
#define TICK(t) button_gestures_tick(&state, t, emit, 0)

int run_tests(void)
{
	/* Idle never emits, and has no wakeup deadline. */
	reset();
	TICK(10000);
	CHECK(count == 0 && button_gestures_deadline(&state) == INT64_MAX);
	++tests_passed;

	/* Single click is delayed until 350 ms after release. */
	reset();
	EDGE(true, 100);
	EDGE(false, 200);
	TICK(549);
	CHECK(count == 0);
	TICK(550);
	CHECK(count == 1 && events[0] == BUTTON_PRESS);
	TICK(5000);
	CHECK(count == 1 && button_gestures_deadline(&state) == INT64_MAX);
	++tests_passed;

	/* Double click emits only double, never a preliminary single. */
	reset();
	EDGE(true, 0); EDGE(false, 60);
	EDGE(true, 200); EDGE(false, 260);
	CHECK(count == 1 && events[0] == BUTTON_DOUBLE_PRESS);
	TICK(5000);
	CHECK(count == 1);
	++tests_passed;

	/* Hold arrives before release, once only even over a very long hold. */
	reset();
	EDGE(true, 100);
	TICK(899); CHECK(count == 0);
	TICK(900); CHECK(count == 1 && events[0] == BUTTON_HOLD);
	CHECK(button_gestures_deadline(&state) == INT64_MAX);
	TICK(100000); CHECK(count == 1);
	EDGE(false, 100100);
	CHECK(count == 2 && events[1] == BUTTON_LONG_RELEASE);
	TICK(110000); CHECK(count == 2);
	++tests_passed;

	/* Releasing exactly at the hold threshold yields hold, then release. */
	reset();
	EDGE(true, 0); EDGE(false, LONG_PRESS_MS);
	CHECK(count == 2 && events[0] == BUTTON_HOLD && events[1] == BUTTON_LONG_RELEASE);
	++tests_passed;

	/* Just below threshold is a short click, not a hold. */
	reset();
	EDGE(true, 0); EDGE(false, LONG_PRESS_MS - 1);
	TICK(LONG_PRESS_MS - 1 + DOUBLE_CLICK_MS);
	CHECK(count == 1 && events[0] == BUTTON_PRESS);
	++tests_passed;

	/* A second press just inside the double-click window is accepted. */
	reset();
	EDGE(true, 0); EDGE(false, 50);
	EDGE(true, 399); EDGE(false, 500);
	CHECK(count == 1 && events[0] == BUTTON_DOUBLE_PRESS);
	++tests_passed;

	/* At the exact deadline, clicks are two independent singles. */
	reset();
	EDGE(true, 0); EDGE(false, 50);
	EDGE(true, 400); EDGE(false, 450); TICK(800);
	CHECK(count == 2 && events[0] == BUTTON_PRESS && events[1] == BUTTON_PRESS);
	++tests_passed;

	/* First short click followed by a held second press becomes a hold. */
	reset();
	EDGE(true, 0); EDGE(false, 50); EDGE(true, 200);
	TICK(1000); EDGE(false, 1100); TICK(5000);
	CHECK(count == 2 && events[0] == BUTTON_HOLD && events[1] == BUTTON_LONG_RELEASE);
	++tests_passed;

	/* Repeated identical input levels never create duplicate actions. */
	reset();
	EDGE(false, 0); EDGE(true, 100); EDGE(true, 200);
	EDGE(false, 300); EDGE(false, 400); TICK(650);
	CHECK(count == 1 && events[0] == BUTTON_PRESS);
	++tests_passed;

	/* Three short clicks group as double + single; no stale hold timer. */
	reset();
	EDGE(true, 0); EDGE(false, 50); EDGE(true, 150); EDGE(false, 200);
	EDGE(true, 250); EDGE(false, 300); TICK(2000);
	CHECK(count == 2 && events[0] == BUTTON_DOUBLE_PRESS && events[1] == BUTTON_PRESS);
	++tests_passed;

	/* A short click after a long-release starts a fresh gesture. */
	reset();
	EDGE(true, 0); TICK(800); EDGE(false, 1000);
	EDGE(true, 1100); EDGE(false, 1150); TICK(1500);
	CHECK(count == 3 && events[0] == BUTTON_HOLD &&
	      events[1] == BUTTON_LONG_RELEASE && events[2] == BUTTON_PRESS);
	++tests_passed;

	/* Uptime beyond 32 bits must not wrap the deadlines. */
	reset();
	const int64_t base = (INT64_C(1) << 32) + 100;
	EDGE(true, base); TICK(base + 799); CHECK(count == 0);
	TICK(base + 800); EDGE(false, base + 900);
	CHECK(count == 2 && events[0] == BUTTON_HOLD && events[1] == BUTTON_LONG_RELEASE);
	++tests_passed;
	return run_packet_tests();
}
