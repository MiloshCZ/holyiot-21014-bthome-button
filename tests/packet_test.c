#include "bthome_packet.h"
#include "button_gestures.h"

extern unsigned tests_passed;
#define CHECK(c) do { if (!(c)) { return 1000 + __LINE__; } } while (0)

int run_packet_tests(void)
{
	uint8_t guarded[BTHOME_PAYLOAD_MAX + 2] = {0};
	uint8_t *packet = &guarded[1];
	guarded[0] = 0xa5;
	guarded[sizeof(guarded) - 1] = 0x5a;
	size_t len = bthome_encode(packet, 7, BUTTON_PRESS, 3000, true);
	const uint8_t expected[] = {0xd2, 0xfc, 0x44, 0x00, 7, 0x0c, 0xb8, 0x0b, 0x3a, 1};
	CHECK(len == sizeof(expected));
	for (size_t i = 0; i < len; ++i) { CHECK(packet[i] == expected[i]); }
	CHECK(guarded[0] == 0xa5 && guarded[sizeof(guarded) - 1] == 0x5a);
	++tests_passed;

	/* A heartbeat after a hold must clear the old event, not repeat it. */
	bthome_encode(packet, 8, BUTTON_HOLD, 2980, true);
	len = bthome_encode(packet, 9, 0, 2990, true);
	CHECK(len == 10 && packet[4] == 9 && packet[9] == 0);
	CHECK(packet[6] == 0xae && packet[7] == 0x0b);
	++tests_passed;

	/* An ADC failure removes voltage completely, preserving the event. */
	len = bthome_encode(packet, 10, BUTTON_LONG_RELEASE, 0, false);
	CHECK(len == 7 && packet[5] == 0x3a && packet[6] == BUTTON_LONG_RELEASE);
	++tests_passed;

	/* Recovery restores the voltage object, in ascending object-ID order. */
	len = bthome_encode(packet, 11, BUTTON_DOUBLE_PRESS, 3600, true);
	CHECK(len == 10 && packet[3] == 0 && packet[5] == 0x0c && packet[8] == 0x3a);
	CHECK(packet[6] == 0x10 && packet[7] == 0x0e && packet[9] == BUTTON_DOUBLE_PRESS);
	++tests_passed;

	/* Packet IDs can wrap without changing the event or voltage fields. */
	bthome_encode(packet, 255, BUTTON_PRESS, 2000, true);
	CHECK(packet[4] == 255);
	bthome_encode(packet, 0, BUTTON_PRESS, 2000, true);
	CHECK(packet[4] == 0 && packet[6] == 0xd0 && packet[7] == 7 && packet[9] == 1);
	++tests_passed;

	len = bthome_encode(packet, 1, 0, 0, false);
	CHECK(len == 7 && packet[5] == 0x3a && packet[6] == 0);
	++tests_passed;
	return 0;
}
