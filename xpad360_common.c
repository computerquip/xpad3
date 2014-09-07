#include <linux/usb/input.h>
#include <linux/module.h>

#include "xpad360_common.h"

MODULE_AUTHOR("Zachary Lund <admin@computerquip.com>");
MODULE_DESCRIPTION("Common Operations between Xbox 360 Devices");
MODULE_LICENSE("GPL");

struct workqueue_struct * xpad360_common_workqueue;

int xpad360_setup_transfer_out(
	struct usb_interface *usb_intf,
	struct xpad360_transfer *transfer,
	usb_complete_t irq, gfp_t mem_flag)
{
	void * user_data = usb_get_intfdata(usb_intf);
	struct usb_device *usb_dev = interface_to_usbdev(usb_intf);
	struct usb_endpoint_descriptor *ep = &usb_intf->cur_altsetting->endpoint[1].desc;
	const int pipe = usb_sndintpipe(usb_dev, ep->bEndpointAddress); 
	int error = xpad360_alloc_transfer(usb_dev, transfer, mem_flag);
	
	if (error) 
		return error;
	
	usb_fill_int_urb(
		transfer->urb, usb_dev,
		pipe, transfer->buffer, 32,
		irq, user_data, ep->bInterval);
	
	return 0;
}

EXPORT_SYMBOL_GPL(xpad360_setup_transfer_out);

int xpad360_setup_transfer_in(
	struct usb_interface *usb_intf,
	struct xpad360_transfer *transfer,
	usb_complete_t irq, gfp_t mem_flag)
{
	void * user_data = usb_get_intfdata(usb_intf);
	struct usb_device *usb_dev = interface_to_usbdev(usb_intf);
	struct usb_endpoint_descriptor *ep = &usb_intf->cur_altsetting->endpoint[0].desc;
	const int pipe = usb_rcvintpipe(usb_dev, ep->bEndpointAddress); 
	int error = xpad360_alloc_transfer(usb_dev, transfer, mem_flag);
	
	if (error) 
		return error;
	
	usb_fill_int_urb(
		transfer->urb, usb_dev,
		pipe, transfer->buffer, 32,
		irq, user_data, ep->bInterval);
	
	return 0;
}

EXPORT_SYMBOL_GPL(xpad360_setup_transfer_in);

void xpad360_free_transfer(struct usb_device *usb_dev, struct xpad360_transfer *transfer)
{
	usb_kill_urb(transfer->urb);
	usb_free_urb(transfer->urb);
	usb_free_coherent(usb_dev, 32, transfer->buffer, transfer->dma);
}

EXPORT_SYMBOL_GPL(xpad360_free_transfer);

struct input_dev *xpad360_create_input_dev(
	struct usb_device *usb_dev,
	const char *name,
	int (* open) (struct input_dev *),
	void (* close) (struct input_dev *))
{
	struct input_dev *input_dev = input_allocate_device();
	if (!input_dev) {
		return NULL;
	}
	
	usb_to_input_id(usb_dev, &input_dev->id);
	
	input_dev->name = name;
	input_dev->open = open;
	input_dev->close = close;
	
	return input_dev;
}

EXPORT_SYMBOL_GPL(xpad360_create_input_dev);

void xpad360_unregister_input_dev(struct input_dev *input_dev)
{
	input_unregister_device(input_dev);
}

void xpad360_free_input_dev(struct input_dev *input_dev) 
{
	input_free_device(input_dev);
}

EXPORT_SYMBOL_GPL(xpad360_unregister_input_dev);
EXPORT_SYMBOL_GPL(xpad360_free_input_dev);



static int __init xpad360_common_init(void)
{
	xpad360_common_workqueue = create_workqueue("xpad360_common");
	
	if (!xpad360_common_workqueue) 
		return -ENOMEM;
	
	return 0;
}

static void __exit xpad360_common_exit(void)
{
	destroy_workqueue(xpad360_common_workqueue);
}

module_init(xpad360_common_init);
module_exit(xpad360_common_exit);