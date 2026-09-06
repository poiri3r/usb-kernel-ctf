// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct kapo_transfer_context {
	uint64_t page;
	uint64_t ops;
	uint32_t offset;
	uint32_t reserved;
};

#define KAPO_FETCH_RECORDS _IOW('K', 1, struct kapo_transfer_context)

int main(void)
{
	struct kapo_transfer_context context = {
		.page = 0xffffea0000000000ULL,
		.ops = 0xffffffff81000000ULL,
	};
	int fd = open("/dev/kapo_ctf0", O_RDONLY);
	int result;

	if (fd < 0) {
		perror("open /dev/kapo_ctf0");
		return 1;
	}
	result = ioctl(fd, KAPO_FETCH_RECORDS, &context);
	if (result < 0)
		perror("KAPO_FETCH_RECORDS");
	else
		printf("received %d bytes\n", result);
	close(fd);
	return result < 0;
}
