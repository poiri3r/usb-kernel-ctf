// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define KAPO_MINOR_BASE 192
#define KAPO_REQUEST_RECORD_COUNT 0x5a
#define KAPO_FETCH_RECORDS _IO('K', 1)
#define KAPO_MAX_RECORDS 128

struct kapo_record {
	__le32 id;
	__le32 length;
	__le64 value;
};

struct kapo_device {
	struct usb_device *udev;
	struct kref ref;
	unsigned int bulk_in_pipe;
	u8 interface_number;
};

static struct usb_driver custom_driver;
static DEFINE_MUTEX(custom_disconnect_mutex);

static void custom_delete(struct kref *ref)
{
	struct kapo_device *device = container_of(ref, struct kapo_device, ref);

	usb_put_dev(device->udev);
	kfree(device);
}

static int custom_open(struct inode *inode, struct file *file)
{
	struct usb_interface *interface;
	struct kapo_device *device;
	int result = -ENODEV;

	mutex_lock(&custom_disconnect_mutex);
	interface = usb_find_interface(&custom_driver, iminor(inode));
	if (!interface)
		goto out;
	device = usb_get_intfdata(interface);
	if (!device || !kref_get_unless_zero(&device->ref))
		goto out;
	file->private_data = device;
	result = 0;
out:
	mutex_unlock(&custom_disconnect_mutex);
	return result;
}

static int custom_release(struct inode *inode, struct file *file)
{
	struct kapo_device *device = file->private_data;

	kref_put(&device->ref, custom_delete);
	return 0;
}

static long custom_ioctl(struct file *file, unsigned int command,
			 unsigned long argument)
{
	struct kapo_device *device = file->private_data;
	struct kapo_record **records;
	__le16 device_count;
	unsigned int receive_length;
	unsigned int count;
	int actual_length;
	int result;

	(void)argument;
	if (command != KAPO_FETCH_RECORDS)
		return -ENOTTY;

	result = usb_control_msg(device->udev,
				 usb_rcvctrlpipe(device->udev, 0),
				 KAPO_REQUEST_RECORD_COUNT,
				 USB_DIR_IN | USB_TYPE_VENDOR |
				 USB_RECIP_INTERFACE, 0,
				 device->interface_number, &device_count,
				 sizeof(device_count), USB_CTRL_GET_TIMEOUT);
	if (result != sizeof(device_count))
		return result < 0 ? result : -EIO;

	count = le16_to_cpu(device_count);
	if (!count || count > KAPO_MAX_RECORDS)
		return -EINVAL;

	/* Intentional CTF bug: pointer table size, record-array receive length. */
	records = kmalloc_array(count, sizeof(*records), GFP_KERNEL_ACCOUNT);
	if (!records)
		return -ENOMEM;
	receive_length = count * sizeof(struct kapo_record);
	result = usb_bulk_msg(device->udev, device->bulk_in_pipe, records,
			      receive_length, &actual_length, 5000);
	kfree(records);
	return result ?: actual_length;
}

static const struct file_operations custom_fops = {
	.owner = THIS_MODULE,
	.open = custom_open,
	.release = custom_release,
	.unlocked_ioctl = custom_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static struct usb_class_driver custom_class = {
	.name = "kapo_ctf%d",
	.fops = &custom_fops,
	.minor_base = KAPO_MINOR_BASE,
};

static int custom_probe(struct usb_interface *interface,
			const struct usb_device_id *id)
{
	struct usb_host_interface *alternate = interface->cur_altsetting;
	struct kapo_device *device;
	u8 endpoint_number;
	int i;
	int result;

	(void)id;
	device = kzalloc(sizeof(*device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;
	device->udev = usb_get_dev(interface_to_usbdev(interface));
	device->interface_number = alternate->desc.bInterfaceNumber;
	kref_init(&device->ref);

	for (i = 0; i < alternate->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *endpoint =
			&alternate->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(endpoint)) {
			endpoint_number = usb_endpoint_num(endpoint);
			device->bulk_in_pipe = usb_rcvbulkpipe(device->udev, endpoint_number);
			break;
		}
	}
	if (!device->bulk_in_pipe) {
		result = -ENODEV;
		goto error;
	}

	usb_set_intfdata(interface, device);
	result = usb_register_dev(interface, &custom_class);
	if (result) {
		usb_set_intfdata(interface, NULL);
		goto error;
	}
	dev_info(&interface->dev, "KAPO CTF interface ready at minor %d\n",
		 interface->minor);
	return 0;

error:
	kref_put(&device->ref, custom_delete);
	return result;
}

static void custom_disconnect(struct usb_interface *interface)
{
	struct kapo_device *device = usb_get_intfdata(interface);

	mutex_lock(&custom_disconnect_mutex);
	usb_set_intfdata(interface, NULL);
	usb_deregister_dev(interface, &custom_class);
	mutex_unlock(&custom_disconnect_mutex);
	kref_put(&device->ref, custom_delete);
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
