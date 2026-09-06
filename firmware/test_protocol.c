#include "kapo_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	uint8_t descriptor[KAPO_CONFIG_DESCRIPTOR_SIZE];
	uint8_t context[KAPO_TRANSFER_CONTEXT_SIZE] = {
		1, 2, 3, 4, 5, 6, 7, 8,
		9, 10, 11, 12, 13, 14, 15, 16,
		0x8f, 0x07, 0, 0,
	};
	uint8_t payload[KAPO_RECORD_PAYLOAD_SIZE];

	assert(kapo_build_device_descriptor(KAPO_MODE_HELPER, descriptor) == 18);
	assert(descriptor[17] == 8);
	assert(kapo_build_config_descriptor(KAPO_MODE_HELPER, 7, descriptor) == 25);
	assert(descriptor[2] == 0 && descriptor[3] == 4);
	assert(descriptor[5] == 8);
	assert(kapo_build_config_descriptor(KAPO_MODE_SEQ, 0, descriptor) == 25);
	assert(descriptor[2] == 32 && descriptor[3] == 0);
	assert(kapo_build_record_payload(payload, context));
	assert(!memcmp(payload + 1024, context, 8));
	assert(!memcmp(payload + 1040, context + 8, 8));
	assert(payload[1032] == 0x8f && payload[1033] == 0x07);
	assert(payload[1036] == 1 && payload[1048] == 0x10);
	context[20] = 1;
	assert(!kapo_build_record_payload(payload, context));
	puts("PASS: RP2040 descriptor and Stage 2 payload protocol");
	return 0;
}
