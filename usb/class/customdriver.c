// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/usb.h>

static int custom_probe(struct usb_interface *interface,
			const struct usb_device_id *id)
{
	dev_info(&interface->dev, "KAPO CTF interface reached probe\n");
	return 0;
}

static void custom_disconnect(struct usb_interface *interface)
{
	dev_info(&interface->dev, "KAPO CTF interface disconnected\n");
}

static const struct usb_device_id custom_id_table[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_VENDOR_SPEC, 0x42, 0x01) },
	{ }
};
MODULE_DEVICE_TABLE(usb, custom_id_table);

static struct usb_driver custom_driver = {
	.name = "kapo_ctf",
	.probe = custom_probe,
	.disconnect = custom_disconnect,
	.id_table = custom_id_table,
};
module_usb_driver(custom_driver);

MODULE_DESCRIPTION("KAPO USB kernel CTF driver");
MODULE_LICENSE("GPL");
