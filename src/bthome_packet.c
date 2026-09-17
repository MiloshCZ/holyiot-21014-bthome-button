#include "bthome_packet.h"

size_t bthome_encode(uint8_t out[BTHOME_PAYLOAD_MAX], uint8_t packet_id,
		     uint8_t event, uint16_t millivolts, bool voltage_valid)
{
	size_t n = 0;
	out[n++] = 0xd2; /* Service UUID 0xFCD2, little-endian. */
	out[n++] = 0xfc;
	out[n++] = 0x44; /* BTHome v2, unencrypted, trigger-based. */
	out[n++] = 0x00;
	out[n++] = packet_id;
	if (voltage_valid) {
		out[n++] = 0x0c; /* uint16 millivolts, displayed in volts by HA. */
		out[n++] = (uint8_t)millivolts;
		out[n++] = (uint8_t)(millivolts >> 8);
	}
	out[n++] = 0x3a;
	out[n++] = event; /* Zero for startup/heartbeat: never repeat an old press. */
	return n;
}
