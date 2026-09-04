// SPDX-License-Identifier: GPL-2.0
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
#define PIPE_COUNT 128
#define SEQ_COUNT 256
#define PIPE_RING_SIZE 16
#define PAGE_SIZE 4096
#define RECORD_COUNT 128
#define RECORD_SIZE 16
#define KAPO_REQUEST_RECORD_COUNT 0x5a
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

static const struct usb_device_descriptor device_descriptor = {
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

static void fail(const char *operation)
{
	perror(operation);
	exit(EXIT_FAILURE);
}

static void groom_pipe_buffers(void)
{
	char page[PAGE_SIZE] = { 0 };
	int pipes[PIPE_COUNT][2];
	int i, j;

	for (i = 0; i < PIPE_COUNT; i++) {
		if (pipe(pipes[i]))
			fail("pipe grooming");
		for (j = 0; j < PIPE_RING_SIZE; j++)
			if (write(pipes[i][1], page, sizeof(page)) != sizeof(page))
				fail("pipe grooming");
	}
	for (i = 0; i < PIPE_COUNT; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
}

static void groom_seq_operations(void)
{
	int files[SEQ_COUNT];
	int i;

	for (i = 0; i < SEQ_COUNT; i++) {
		files[i] = open("/proc/self/stat", O_RDONLY);
		if (files[i] < 0)
			fail("seq_operations grooming");
	}
	for (i = 0; i < SEQ_COUNT; i++)
		close(files[i]);
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

static void *send_records(void *argument)
{
	struct {
		struct usb_raw_ep_io io;
		uint8_t data[RECORD_COUNT * RECORD_SIZE];
	} transfer = { .io.length = sizeof(transfer.data) };
	int fd = (int)(intptr_t)argument;

	transfer.io.ep = bulk_in_endpoint;
	memset(transfer.data, 'A', sizeof(transfer.data));
	if (ioctl(fd, USB_RAW_IOCTL_EP_WRITE, &transfer) < 0)
		fail("USB_RAW_IOCTL_EP_WRITE");
	return NULL;
}

int main(int argc, char **argv)
{
	struct usb_raw_init init = { .speed = USB_SPEED_HIGH };
	struct control_event event;
	int fd;

	strcpy((char *)init.driver_name, "dummy_udc");
	strcpy((char *)init.device_name, "dummy_udc.0");
	fd = open("/dev/raw-gadget", O_RDWR);
	if (fd < 0)
		fail("open /dev/raw-gadget");
	if (argc == 2 && !strcmp(argv[1], "seq")) {
		short_configuration.configuration.wTotalLength =
			htole16(SEQ_CONFIG_LENGTH);
		groom_seq_operations();
	} else {
		groom_pipe_buffers();
	}
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
			reply(fd, &short_configuration, SHORT_CONFIGURATION_LENGTH,
			      length);
		} else if (event.control.bRequest == USB_REQ_SET_CONFIGURATION) {
			pthread_t thread;

			bulk_in_endpoint = ioctl(fd, USB_RAW_IOCTL_EP_ENABLE,
						 &short_configuration.endpoint);
			if (bulk_in_endpoint < 0)
				fail("USB_RAW_IOCTL_EP_ENABLE");
			if (ioctl(fd, USB_RAW_IOCTL_CONFIGURE) < 0)
				fail("USB_RAW_IOCTL_CONFIGURE");
			acknowledge_out(fd);
			if (pthread_create(&thread, NULL, send_records,
					   (void *)(intptr_t)fd))
				fail("pthread_create");
			pthread_detach(thread);
		} else if ((event.control.bRequestType & USB_TYPE_MASK) ==
			   USB_TYPE_VENDOR &&
			   event.control.bRequest == KAPO_REQUEST_RECORD_COUNT) {
			const uint16_t count = htole16(RECORD_COUNT);

			reply(fd, &count, sizeof(count), length);
		} else {
			if (ioctl(fd, USB_RAW_IOCTL_EP0_STALL) < 0 && errno != EBUSY)
				fail("USB_RAW_IOCTL_EP0_STALL");
		}
	}
}
