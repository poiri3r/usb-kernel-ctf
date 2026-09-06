#include "kapo_protocol.h"

#include <string.h>

#define KAPO_PIPE_BUFFER_OFFSET 1024
#define KAPO_PIPE_PAGE_OFFSET 0
#define KAPO_PIPE_OFFSET_OFFSET 8
#define KAPO_PIPE_LEN_OFFSET 12
#define KAPO_PIPE_OPS_OFFSET 16
#define KAPO_PIPE_FLAGS_OFFSET 24
#define KAPO_PIPE_PRIVATE_OFFSET 32

static void put_le16(uint8_t *output, uint16_t value)
{
	output[0] = value;
	output[1] = value >> 8;
}

static void put_le32(uint8_t *output, uint32_t value)
{
	output[0] = value;
	output[1] = value >> 8;
	output[2] = value >> 16;
	output[3] = value >> 24;
}

static uint32_t get_le32(const uint8_t *input)
{
	return (uint32_t)input[0] | (uint32_t)input[1] << 8 |
	       (uint32_t)input[2] << 16 | (uint32_t)input[3] << 24;
}

static bool all_zero(const uint8_t *input, size_t length)
{
	while (length--)
		if (*input++)
			return false;
	return true;
}

size_t kapo_build_device_descriptor(enum kapo_mode mode, uint8_t *output)
{
	static const uint8_t descriptor[KAPO_DEVICE_DESCRIPTOR_SIZE] = {
		18, 1, 0x00, 0x02, 0, 0, 0, 64,
		0xfe, 0xca, 0x01, 0x68, 0x00, 0x01, 0, 0, 0, 1,
	};

	memcpy(output, descriptor, sizeof(descriptor));
	output[17] = mode == KAPO_MODE_HELPER ? 8 : 1;
	return sizeof(descriptor);
}

size_t kapo_build_config_descriptor(enum kapo_mode mode, uint8_t index,
				    uint8_t *output)
{
	static const uint8_t descriptor[KAPO_CONFIG_DESCRIPTOR_SIZE] = {
		9, 2, 0, 0, 1, 1, 0, 0xc0, 0x32,
		9, 4, 0, 0, 1, 0xff, 0x42, 0x01, 0,
		7, 5, 0x81, 2, 64, 0, 0,
	};
	uint16_t advertised_length = mode == KAPO_MODE_SEQ ? 32 : 1024;

	memcpy(output, descriptor, sizeof(descriptor));
	put_le16(output + 2, advertised_length);
	output[5] = index + 1;
	return sizeof(descriptor);
}

bool kapo_build_record_payload(uint8_t *payload, const uint8_t *context)
{
	uint8_t *entry = payload + KAPO_PIPE_BUFFER_OFFSET;
	uint32_t offset = get_le32(context + 16);

	if (all_zero(context, 8) || all_zero(context + 8, 8) ||
	    offset > 4094 || !all_zero(context + 20, 4))
		return false;
	memset(payload, 0, KAPO_RECORD_PAYLOAD_SIZE);
	memcpy(entry + KAPO_PIPE_PAGE_OFFSET, context, 8);
	memcpy(entry + KAPO_PIPE_OFFSET_OFFSET, context + 16, 4);
	put_le32(entry + KAPO_PIPE_LEN_OFFSET, 1);
	memcpy(entry + KAPO_PIPE_OPS_OFFSET, context + 8, 8);
	put_le32(entry + KAPO_PIPE_FLAGS_OFFSET, 0x10);
	memset(entry + KAPO_PIPE_PRIVATE_OFFSET, 0, 8);
	return true;
}
