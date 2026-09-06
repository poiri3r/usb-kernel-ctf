#ifndef KAPO_PROTOCOL_H
#define KAPO_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KAPO_DEVICE_DESCRIPTOR_SIZE 18
#define KAPO_CONFIG_DESCRIPTOR_SIZE 25
#define KAPO_RECORD_PAYLOAD_SIZE 2048
#define KAPO_TRANSFER_CONTEXT_SIZE 24

enum kapo_mode {
	KAPO_MODE_HELPER,
	KAPO_MODE_SEQ,
	KAPO_MODE_STAGE2,
	KAPO_MODE_COUNT,
};

size_t kapo_build_device_descriptor(enum kapo_mode mode, uint8_t *output);
size_t kapo_build_config_descriptor(enum kapo_mode mode, uint8_t index,
				    uint8_t *output);
bool kapo_build_record_payload(uint8_t *payload, const uint8_t *context);

#endif
