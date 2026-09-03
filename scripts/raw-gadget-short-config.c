// SPDX-License-Identifier: GPL-2.0
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <linux/usb/raw_gadget.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CONFIG_LENGTH 1024
#define PIPE_COUNT 128
#define IO_DATA_LENGTH CONFIG_LENGTH

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

static const struct {
	struct usb_config_descriptor configuration;
	struct usb_interface_descriptor interface;
} __attribute__((packed)) short_configuration = {
	.configuration = {
		.bLength = USB_DT_CONFIG_SIZE,
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = __constant_cpu_to_le16(CONFIG_LENGTH),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	},
	.interface = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 0x42,
		.bInterfaceProtocol = 0x01,
	},
};

static void fail(const char *operation)
{
	perror(operation);
	exit(EXIT_FAILURE);
}

static void groom_pipe_buffers(void)
{
	int pipes[PIPE_COUNT][2];
	int i;

	for (i = 0; i < PIPE_COUNT; i++) {
		if (pipe(pipes[i]) || write(pipes[i][1], "x", 1) != 1)
			fail("pipe grooming");
	}
	for (i = 0; i < PIPE_COUNT; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
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

int main(void)
{
	struct usb_raw_init init = { .speed = USB_SPEED_HIGH };
	struct control_event event;
	int fd;

	strcpy((char *)init.driver_name, "dummy_udc");
	strcpy((char *)init.device_name, "dummy_udc.0");
	groom_pipe_buffers();

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
			reply(fd, &short_configuration, sizeof(short_configuration),
			      length);
		} else if (event.control.bRequest == USB_REQ_SET_CONFIGURATION) {
			if (ioctl(fd, USB_RAW_IOCTL_CONFIGURE) < 0)
				fail("USB_RAW_IOCTL_CONFIGURE");
			acknowledge_out(fd);
		} else {
			if (ioctl(fd, USB_RAW_IOCTL_EP0_STALL) < 0 && errno != EBUSY)
				fail("USB_RAW_IOCTL_EP0_STALL");
		}
	}
}
