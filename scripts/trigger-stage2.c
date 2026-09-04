// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KAPO_FETCH_RECORDS _IO('K', 1)

int main(void)
{
	int fd = open("/dev/kapo_ctf0", O_RDONLY);
	int result;

	if (fd < 0) {
		perror("open /dev/kapo_ctf0");
		return 1;
	}
	result = ioctl(fd, KAPO_FETCH_RECORDS);
	if (result < 0)
		perror("KAPO_FETCH_RECORDS");
	else
		printf("received %d bytes\n", result);
	close(fd);
	return result < 0;
}
