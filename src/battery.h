#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

/* VDD is the CR2032 voltage only when powered by the cell, not the programmer. */
int battery_read_mv(uint16_t *millivolts, bool recalibrate);

#endif
