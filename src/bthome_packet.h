#ifndef BTHOME_PACKET_H
#define BTHOME_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BTHOME_PAYLOAD_MAX 10
size_t bthome_encode(uint8_t out[BTHOME_PAYLOAD_MAX], uint8_t packet_id,
		     uint8_t event, uint16_t millivolts, bool voltage_valid);

#endif
