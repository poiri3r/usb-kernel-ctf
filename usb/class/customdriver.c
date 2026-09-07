// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#define KAPO_MINOR_BASE 192
#define KAPO_REQUEST_RECORD_COUNT 0x5a
#define KAPO_REQUEST_TRANSFER_CONTEXT 0x5b
#define KAPO_REQUEST_NEXT_STAGE 0x5c
#define KAPO_MAX_RECORDS 128

struct kapo_transfer_context {
	__u64 page;
	__u64 ops;
	__u32 offset;
	__u32 reserved;
};

#define KAPO_FETCH_RECORDS _IOW('K', 1, struct kapo_transfer_context)
#define KAPO_NEXT_STAGE _IO('K', 2)

struct kapo_record {
	__le32 id;
	__le32 length;
	__le64 value;
};

struct kapo_device {
	struct usb_device *udev;
	struct kref ref;
	struct mutex io_mutex;
	unsigned int bulk_in_pipe;
	u8 interface_number;
};

struct kapo_batch {
	unsigned int count;
	struct kapo_record *records;
};

struct kapo_wire_context {
	__le64 page;
	__le64 ops;
	__le32 offset;
	__le32 reserved;
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

static int custom_receive_batch(struct kapo_device *device,
				struct kapo_batch *batch)
{
	unsigned int length = batch->count * sizeof(*batch->records);
	int actual_length;
	int result;

	result = usb_bulk_msg(device->udev, device->bulk_in_pipe,
			      batch->records, length, &actual_length, 5000);
	return result ?: actual_length;
}

static long custom_ioctl(struct file *file, unsigned int command,
			 unsigned long argument)
{
	struct kapo_device *device = file->private_data;
	struct kapo_transfer_context context;
	struct kapo_wire_context wire_context;
	struct kapo_batch batch;
	__le16 device_count;
	unsigned int count;
	int result;

	if (command == KAPO_NEXT_STAGE) {
		mutex_lock(&device->io_mutex);
		result = usb_control_msg_send(device->udev, 0,
					      KAPO_REQUEST_NEXT_STAGE,
					      USB_DIR_OUT | USB_TYPE_VENDOR |
					      USB_RECIP_INTERFACE, 0,
					      device->interface_number, NULL, 0,
					      USB_CTRL_SET_TIMEOUT,
					      GFP_KERNEL);
		mutex_unlock(&device->io_mutex);
		return result;
	}
	if (command != KAPO_FETCH_RECORDS)
		return -ENOTTY;
	if (copy_from_user(&context, (void __user *)argument, sizeof(context)))
		return -EFAULT;
	if (!context.page || !context.ops || context.offset > PAGE_SIZE - 2 ||
	    context.reserved)
		return -EINVAL;
	wire_context.page = cpu_to_le64(context.page);
	wire_context.ops = cpu_to_le64(context.ops);
	wire_context.offset = cpu_to_le32(context.offset);
	wire_context.reserved = 0;
	mutex_lock(&device->io_mutex);
	result = usb_control_msg_send(device->udev, 0,
				      KAPO_REQUEST_TRANSFER_CONTEXT,
				      USB_DIR_OUT | USB_TYPE_VENDOR |
				      USB_RECIP_INTERFACE, 0,
				      device->interface_number, &wire_context,
				      sizeof(wire_context), USB_CTRL_SET_TIMEOUT,
				      GFP_KERNEL);
	if (result)
		goto out_unlock;

	result = usb_control_msg(device->udev,
				 usb_rcvctrlpipe(device->udev, 0),
				 KAPO_REQUEST_RECORD_COUNT,
				 USB_DIR_IN | USB_TYPE_VENDOR |
				 USB_RECIP_INTERFACE, 0,
				 device->interface_number, &device_count,
				 sizeof(device_count), USB_CTRL_GET_TIMEOUT);
	if (result != sizeof(device_count)) {
		result = result < 0 ? result : -EIO;
		goto out_unlock;
	}

	count = le16_to_cpu(device_count);
	if (!count || count > KAPO_MAX_RECORDS) {
		result = -EINVAL;
		goto out_unlock;
	}

	batch.count = count;
	batch.records = kmalloc_array(batch.count, sizeof(batch.records),
				      GFP_KERNEL_ACCOUNT);
	if (!batch.records) {
		result = -ENOMEM;
		goto out_unlock;
	}
	result = custom_receive_batch(device, &batch);
	kfree(batch.records);
out_unlock:
	mutex_unlock(&device->io_mutex);
	return result;
}

static const struct file_operations custom_fops = {
	.owner = THIS_MODULE,
	.open = custom_open,
	.release = custom_release,
	.unlocked_ioctl = custom_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static char *custom_devnode(const struct device *dev, umode_t *mode)
{
	(void)dev;
	if (mode)
		*mode = 0666;
	return NULL;
}

static struct usb_class_driver custom_class = {
	.name = "kapo_ctf%d",
	.devnode = custom_devnode,
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
	mutex_init(&device->io_mutex);

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
