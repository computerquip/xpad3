#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb/input.h>
#include <linux/workqueue.h>

#include "xpad360_common.h"

MODULE_AUTHOR("Zachary Lund <admin@computerquip.com>");
MODULE_DESCRIPTION("Xbox 360 Wireless Adapter");
MODULE_LICENSE("GPL");

struct workqueue_struct *xpad360wr_workqueue;

static const int xpad360wr_keybit[] = {
	BTN_A, BTN_B, BTN_X, BTN_Y,
	BTN_START, BTN_SELECT,
	BTN_THUMBL, BTN_THUMBR,
	BTN_TL, BTN_TR, BTN_MODE,
	BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY2, 
	BTN_TRIGGER_HAPPY3, BTN_TRIGGER_HAPPY4
};

static const int xpad360wr_absbit[] = {
	ABS_X, ABS_RX, 
	ABS_Y, ABS_RY,
	ABS_Z, ABS_RZ
};

struct xpad360wr_controller {
	struct xpad360_transfer in;
	struct xpad360_transfer led_out;
	struct xpad360_transfer rumble_out;
	struct input_dev *input_dev;
};

static const char* xpad360wr_device_names[] = {
	"Xbox 360 Wireless Adapter",
};

static const struct usb_device_id xpad360wr_table[] = {
	{ USB_DEVICE_INTERFACE_PROTOCOL(0x045E, 0x0719, 129) },
	{}
};

static int xpad360wr_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int error;
	
	struct xpad360wr_controller *controller =
		kzalloc(sizeof(struct xpad360wr_controller), GFP_KERNEL);
		
	if (!controller)
		return -ENOMEM;
	
	usb_set_intfdata(interface, controller);
	
	return error;
}

static void xpad360wr_disconnect(struct usb_interface *interface)
{
	struct xpad360wr_controller *controller = 
		usb_get_intfdata(interface);
		
	kfree(controller);
}

struct usb_driver xpad360wr_driver = {
	.name		= "xpad360wr",
	.probe		= xpad360wr_probe,
	.disconnect	= xpad360wr_disconnect,
	.id_table	= xpad360wr_table,
	.soft_unbind	= 1
};

static int __init xpad360wr_init(void)
{
	xpad360wr_workqueue = create_workqueue("xpad360wr");
	
	if (!xpad360wr_workqueue) 
		return -ENOMEM;
	
	return usb_register(&xpad360wr_driver);
}

static void __exit xpad360wr_exit(void)
{
	usb_deregister(&xpad360wr_driver);
	destroy_workqueue(xpad360wr_workqueue);
	xpad360wr_workqueue = NULL;
}

MODULE_DEVICE_TABLE(usb, xpad360wr_table);
module_init(xpad360wr_init);
module_exit(xpad360wr_exit);