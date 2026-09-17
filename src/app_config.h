#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* HOLYIOT 21011 v1.0 is compatible with the confirmed nRF52810 wiring:
 * button P0.31 (package pin 43), active HIGH; red LED P0.29 (pin 41),
 * active LOW. Use red for its single LED. 21014 also supports green/blue.
 * Only the selected LED GPIO is configured and driven.
 */
#define LED_COLOR_RED 0
#define LED_COLOR_GREEN 1
#define LED_COLOR_BLUE 2
#ifndef LED_COLOR
#define LED_COLOR LED_COLOR_RED
#endif

#define DEBOUNCE_MS 25
#define EVENT_ADVERTISING_MS 750
#define MIN_EVENT_AIRTIME_MS 350
#define HEARTBEAT_ADVERTISING_MS 1200
#define HEARTBEAT_INTERVAL_MS (INT64_C(6) * 60 * 60 * 1000)
#define DISCOVERY_MS 10000
#define LED_MS 8

#endif
