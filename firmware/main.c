#include "kapo_protocol.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware/irq.h"
#include "hardware/regs/usb.h"
#include "hardware/resets.h"
#include "hardware/structs/usb.h"
#include "pico/stdlib.h"

#define USB_DIR_OUT 0x00
#define USB_DIR_IN 0x80
#define USB_TYPE_MASK 0x60
#define USB_TYPE_STANDARD 0x00
#define USB_TYPE_VENDOR 0x40
#define USB_RECIP_INTERFACE 0x01
#define USB_REQUEST_GET_STATUS 0x00
#define USB_REQUEST_SET_ADDRESS 0x05
#define USB_REQUEST_GET_DESCRIPTOR 0x06
#define USB_REQUEST_GET_CONFIGURATION 0x08
#define USB_REQUEST_SET_CONFIGURATION 0x09
#define USB_REQUEST_GET_INTERFACE 0x0a
#define USB_REQUEST_SET_INTERFACE 0x0b
#define USB_DESCRIPTOR_DEVICE 0x01
#define USB_DESCRIPTOR_CONFIGURATION 0x02
#define USB_TRANSFER_CONTROL 0
#define USB_TRANSFER_BULK 2
#define KAPO_REQUEST_RECORD_COUNT 0x5a
#define KAPO_REQUEST_TRANSFER_CONTEXT 0x5b
#define KAPO_REQUEST_NEXT_STAGE 0x5c
#define EP0_OUT 0x00
#define EP0_IN 0x80
#define EP1_IN 0x81
#define EP_MAX_PACKET 64

#define usb_hw_set ((usb_hw_t *)hw_set_alias_untyped(usb_hw))
#define usb_hw_clear ((usb_hw_t *)hw_clear_alias_untyped(usb_hw))

struct usb_setup_packet {
	uint8_t request_type;
	uint8_t request;
	uint16_t value;
	uint16_t index;
	uint16_t length;
} __attribute__((packed));

struct endpoint {
	uint8_t address;
	uint8_t transfer_type;
	volatile uint32_t *control;
	volatile uint32_t *buffer_control;
	volatile uint8_t *buffer;
	uint8_t next_pid;
};

enum ep0_out_action {
	EP0_OUT_STATUS,
	EP0_OUT_CONTEXT,
};

static struct endpoint endpoints[] = {
	{
		.address = EP0_OUT,
		.transfer_type = USB_TRANSFER_CONTROL,
		.buffer_control = &usb_dpram->ep_buf_ctrl[0].out,
		.buffer = usb_dpram->ep0_buf_a,
	},
	{
		.address = EP0_IN,
		.transfer_type = USB_TRANSFER_CONTROL,
		.buffer_control = &usb_dpram->ep_buf_ctrl[0].in,
		.buffer = usb_dpram->ep0_buf_a,
	},
	{
		.address = EP1_IN,
		.transfer_type = USB_TRANSFER_BULK,
		.control = &usb_dpram->ep_ctrl[0].in,
		.buffer_control = &usb_dpram->ep_buf_ctrl[1].in,
		.buffer = &usb_dpram->epx_data[0],
	},
};

static enum kapo_mode active_mode = KAPO_MODE_HELPER;
static volatile enum ep0_out_action ep0_out_action;
static volatile bool set_address_pending;
static volatile bool switch_after_status;
static volatile bool switch_pending;
static volatile bool bulk_pending;
static uint8_t device_address;
static uint8_t configured_value;
static uint8_t ep0_buffer[EP_MAX_PACKET];
static uint8_t record_payload[KAPO_RECORD_PAYLOAD_SIZE];
static size_t bulk_offset;

static struct endpoint *find_endpoint(uint8_t address)
{
	for (size_t index = 0; index < sizeof(endpoints) / sizeof(endpoints[0]); index++)
		if (endpoints[index].address == address)
			return &endpoints[index];
	return NULL;
}

static bool endpoint_is_in(const struct endpoint *endpoint)
{
	return endpoint->address & USB_DIR_IN;
}

static void start_transfer(struct endpoint *endpoint, const uint8_t *data,
			   uint16_t length)
{
	uint32_t value = length | USB_BUF_CTRL_AVAIL;

	assert(length <= EP_MAX_PACKET);
	if (endpoint_is_in(endpoint)) {
		if (length)
			memcpy((void *)endpoint->buffer, data, length);
		value |= USB_BUF_CTRL_FULL;
	}
	if (endpoint->next_pid)
		value |= USB_BUF_CTRL_DATA1_PID;
	endpoint->next_pid ^= 1;
	*endpoint->buffer_control = value;
}

static void ep0_reply(const uint8_t *data, uint16_t length)
{
	struct endpoint *endpoint = find_endpoint(EP0_IN);

	endpoint->next_pid = 1;
	start_transfer(endpoint, data, length);
}

static void ep0_acknowledge(void)
{
	ep0_reply(NULL, 0);
}

static void ep0_stall(void)
{
	usb_hw_set->ep_stall_arm = USB_EP_STALL_ARM_EP0_IN_BITS |
				   USB_EP_STALL_ARM_EP0_OUT_BITS;
	usb_dpram->ep_buf_ctrl[0].in = USB_BUF_CTRL_STALL;
	usb_dpram->ep_buf_ctrl[0].out = USB_BUF_CTRL_STALL;
}

static void send_next_bulk_packet(void)
{
	size_t remaining = sizeof(record_payload) - bulk_offset;
	uint16_t length = remaining > EP_MAX_PACKET ? EP_MAX_PACKET : remaining;

	start_transfer(find_endpoint(EP1_IN), record_payload + bulk_offset, length);
}

static void handle_standard_request(const struct usb_setup_packet *setup)
{
	uint8_t descriptor_type = setup->value >> 8;
	uint8_t descriptor_index = setup->value;
	uint16_t length;

	if (setup->request_type == USB_DIR_IN &&
	    setup->request == USB_REQUEST_GET_DESCRIPTOR) {
		if (descriptor_type == USB_DESCRIPTOR_DEVICE) {
			length = kapo_build_device_descriptor(active_mode, ep0_buffer);
		} else if (descriptor_type == USB_DESCRIPTOR_CONFIGURATION &&
			   descriptor_index < (active_mode == KAPO_MODE_HELPER ? 8 : 1)) {
			length = kapo_build_config_descriptor(active_mode,
						      descriptor_index, ep0_buffer);
		} else {
			ep0_stall();
			return;
		}
		if (length > setup->length)
			length = setup->length;
		ep0_reply(ep0_buffer, length);
	} else if (setup->request_type == USB_DIR_OUT &&
		   setup->request == USB_REQUEST_SET_ADDRESS && !setup->length) {
		device_address = setup->value;
		set_address_pending = true;
		ep0_acknowledge();
	} else if (setup->request_type == USB_DIR_OUT &&
		   setup->request == USB_REQUEST_SET_CONFIGURATION && !setup->length) {
		configured_value = setup->value;
		find_endpoint(EP1_IN)->next_pid = 0;
		ep0_acknowledge();
	} else if (setup->request_type == USB_DIR_IN &&
		   setup->request == USB_REQUEST_GET_CONFIGURATION && setup->length) {
		ep0_reply(&configured_value, 1);
	} else if (setup->request_type == USB_DIR_IN &&
		   setup->request == USB_REQUEST_GET_INTERFACE && setup->length) {
		ep0_buffer[0] = 0;
		ep0_reply(ep0_buffer, 1);
	} else if (setup->request_type == USB_DIR_OUT &&
		   setup->request == USB_REQUEST_SET_INTERFACE && !setup->length) {
		ep0_acknowledge();
	} else if (setup->request_type == USB_DIR_IN &&
		   setup->request == USB_REQUEST_GET_STATUS && setup->length >= 2) {
		ep0_buffer[0] = 0;
		ep0_buffer[1] = 0;
		ep0_reply(ep0_buffer, 2);
	} else {
		ep0_stall();
	}
}

static void handle_vendor_request(const struct usb_setup_packet *setup)
{
	if (setup->request_type == (USB_DIR_OUT | USB_TYPE_VENDOR |
				   USB_RECIP_INTERFACE) &&
	    setup->request == KAPO_REQUEST_TRANSFER_CONTEXT &&
	    setup->length == KAPO_TRANSFER_CONTEXT_SIZE &&
	    active_mode == KAPO_MODE_STAGE2) {
		ep0_out_action = EP0_OUT_CONTEXT;
		find_endpoint(EP0_OUT)->next_pid = 1;
		start_transfer(find_endpoint(EP0_OUT), NULL, setup->length);
	} else if (setup->request_type == (USB_DIR_IN | USB_TYPE_VENDOR |
					 USB_RECIP_INTERFACE) &&
		   setup->request == KAPO_REQUEST_RECORD_COUNT &&
		   setup->length >= 2 && active_mode == KAPO_MODE_STAGE2) {
		ep0_buffer[0] = 128;
		ep0_buffer[1] = 0;
		bulk_pending = true;
		ep0_reply(ep0_buffer, 2);
	} else if (setup->request_type == (USB_DIR_OUT | USB_TYPE_VENDOR |
					 USB_RECIP_INTERFACE) &&
		   setup->request == KAPO_REQUEST_NEXT_STAGE && !setup->length &&
		   active_mode < KAPO_MODE_STAGE2) {
		switch_after_status = true;
		ep0_acknowledge();
	} else {
		ep0_stall();
	}
}

static void handle_setup(void)
{
	struct usb_setup_packet setup;

	memcpy(&setup, (const void *)&usb_dpram->setup_packet, sizeof(setup));
	find_endpoint(EP0_IN)->next_pid = 1;
	if ((setup.request_type & USB_TYPE_MASK) == USB_TYPE_STANDARD)
		handle_standard_request(&setup);
	else if ((setup.request_type & USB_TYPE_MASK) == USB_TYPE_VENDOR)
		handle_vendor_request(&setup);
	else
		ep0_stall();
}

static void handle_ep0_in(void)
{
	if (set_address_pending) {
		usb_hw->dev_addr_ctrl = device_address;
		set_address_pending = false;
	} else if (switch_after_status) {
		switch_after_status = false;
		switch_pending = true;
	} else {
		ep0_out_action = EP0_OUT_STATUS;
		start_transfer(find_endpoint(EP0_OUT), NULL, 0);
	}
	if (bulk_pending) {
		bulk_pending = false;
		bulk_offset = 0;
		send_next_bulk_packet();
	}
}

static void handle_ep0_out(uint16_t length)
{
	if (ep0_out_action == EP0_OUT_CONTEXT &&
	    length == KAPO_TRANSFER_CONTEXT_SIZE &&
	    kapo_build_record_payload(record_payload,
				      (const uint8_t *)find_endpoint(EP0_OUT)->buffer)) {
		ep0_out_action = EP0_OUT_STATUS;
		ep0_acknowledge();
	} else if (ep0_out_action == EP0_OUT_CONTEXT) {
		ep0_stall();
	}
}

static void handle_ep1_in(uint16_t length)
{
	bulk_offset += length;
	if (bulk_offset < sizeof(record_payload))
		send_next_bulk_packet();
}

static void handle_buffer_done(uint8_t endpoint_number, bool in)
{
	uint8_t address = endpoint_number | (in ? USB_DIR_IN : 0);
	struct endpoint *endpoint = find_endpoint(address);
	uint16_t length;

	if (!endpoint)
		return;
	length = *endpoint->buffer_control & USB_BUF_CTRL_LEN_MASK;
	if (address == EP0_IN)
		handle_ep0_in();
	else if (address == EP0_OUT)
		handle_ep0_out(length);
	else if (address == EP1_IN)
		handle_ep1_in(length);
}

static void handle_buffer_status(void)
{
	uint32_t remaining = usb_hw->buf_status;

	for (uint8_t bit_index = 0; remaining && bit_index < 32; bit_index++) {
		uint32_t bit = 1u << bit_index;

		if (!(remaining & bit))
			continue;
		usb_hw_clear->buf_status = bit;
		handle_buffer_done(bit_index >> 1, !(bit_index & 1));
		remaining &= ~bit;
	}
}

static void handle_bus_reset(void)
{
	device_address = 0;
	configured_value = 0;
	set_address_pending = false;
	bulk_pending = false;
	bulk_offset = 0;
	usb_hw->dev_addr_ctrl = 0;
	for (size_t index = 0; index < sizeof(endpoints) / sizeof(endpoints[0]); index++)
		endpoints[index].next_pid = 0;
}

void isr_usbctrl(void)
{
	uint32_t status = usb_hw->ints;

	if (status & USB_INTS_SETUP_REQ_BITS) {
		usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
		handle_setup();
	}
	if (status & USB_INTS_BUFF_STATUS_BITS)
		handle_buffer_status();
	if (status & USB_INTS_BUS_RESET_BITS) {
		usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;
		handle_bus_reset();
	}
}

static uint32_t buffer_offset(const volatile uint8_t *buffer)
{
	return (uintptr_t)buffer - (uintptr_t)usb_dpram;
}

static void setup_endpoint(struct endpoint *endpoint)
{
	if (!endpoint->control)
		return;
	*endpoint->control = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
		(endpoint->transfer_type << EP_CTRL_BUFFER_TYPE_LSB) |
		buffer_offset(endpoint->buffer);
}

static void usb_connect(void)
{
	reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
	memset(usb_dpram, 0, sizeof(*usb_dpram));
	irq_set_exclusive_handler(USBCTRL_IRQ, isr_usbctrl);
	irq_set_enabled(USBCTRL_IRQ, true);
	usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
	usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS |
		      USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
	usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;
	usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
	usb_hw->inte = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS |
		       USB_INTS_SETUP_REQ_BITS;
	for (size_t index = 0; index < sizeof(endpoints) / sizeof(endpoints[0]); index++)
		setup_endpoint(&endpoints[index]);
	usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

static void switch_mode(void)
{
	irq_set_enabled(USBCTRL_IRQ, false);
	usb_hw_clear->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
	sleep_ms(2000);
	active_mode++;
	switch_pending = false;
	usb_connect();
}

int main(void)
{
	usb_connect();
	while (true) {
		if (switch_pending)
			switch_mode();
		tight_loop_contents();
	}
}
