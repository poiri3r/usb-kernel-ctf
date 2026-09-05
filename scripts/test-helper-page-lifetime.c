// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HELPER_EXEC_PAGE_OFFSET 4096

int main(int argc, char **argv)
{
	char expected, observed;
	loff_t offset = HELPER_EXEC_PAGE_OFFSET;
	int pipefd[2];
	int source;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <helper-file>\n", argv[0]);
		return EXIT_FAILURE;
	}
	source = open(argv[1], O_RDONLY);
	if (source < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}
	if (pread(source, &expected, sizeof(expected), offset) != sizeof(expected) ||
	    pipe(pipefd) ||
	    splice(source, &offset, pipefd[1], NULL, sizeof(expected), 0) !=
		sizeof(expected)) {
		perror("helper page reference");
		return EXIT_FAILURE;
	}
	if (close(source)) {
		perror("close helper");
		return EXIT_FAILURE;
	}
	printf("LIVE_PIPE_READY byte=%02x\n", (unsigned char)expected);
	if (sleep(10)) {
		perror("sleep");
		return EXIT_FAILURE;
	}
	if (read(pipefd[0], &observed, sizeof(observed)) != sizeof(observed)) {
		perror("live helper pipe");
		return EXIT_FAILURE;
	}
	if (observed != expected) {
		fprintf(stderr, "live helper pipe changed byte\n");
		return EXIT_FAILURE;
	}
	puts("LIVE_PIPE_RETAINED");
	return EXIT_SUCCESS;
}
