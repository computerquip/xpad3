#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb/input.h>

#include "xpad360_common.h"

MODULE_AUTHOR("Zachary Lund <admin@computerquip.com>");
MODULE_DESCRIPTION("Xbox 360 Wired Controller Driver");
MODULE_LICENSE("GPL");

/* Alright... so the goal here is cleanliness and path clarity.
   Not neccessarily optimization. That will come later. 
   We also want commit clarity. Nothing messy. We should take
   care to squash related commits so its easier to squelch 
   hard to find bugs caused by a certain commit. 
   
   I will get this right somehow, someway.*/

static const int xpad360_keybit[] = {
	BTN_A, BTN_B, BTN_X, BTN_Y,
	BTN_START, BTN_SELECT,
	BTN_THUMBL, BTN_THUMBR,
	BTN_TL, BTN_TR, BTN_MODE
};

static const int xpad360_absbit[] = {
	ABS_X, ABS_RX, 
	ABS_Y, ABS_RY,
	ABS_Z, ABS_RZ, 
	ABS_HAT0X, ABS_HAT0Y
};

static const int xpad360_ffbit[] = { FF_RUMBLE };

/* This array corresponds exactly to xpad360_table. */
static const char* xpad360_device_names[] = {
	"Xbox 360 Wired Controller",
};

static struct usb_device_id xpad360_table[] = {
	{ USB_DEVICE_INTERFACE_PROTOCOL(0x045E, 0x028e, 1) },
	{}
};

struct xpad360_abs_params {
	unsigned int axis;
	int min; int max;
	int fuzz; int flat;
};

/* This array corresponds exactly to xpad360_absbit */
static const struct xpad360_abs_params xpad360_abs_params[] = {
	{ -32768, 32768, 16, 128 },
	{ -32768, 32768, 16, 128 },
	{ -32768, 32768, 16, 128 }, 
	{ -32768, 32768, 16, 128 },
	{ 0, 255, 0, 0 },
	{ 0, 255, 0, 0 },
	{ -1, 1, 0, 0 },
	{ -1, 1, 0, 0 }
};

inline static void xpad360_set_abs_params(struct input_dev *input_dev)
{
	const size_t size = (sizeof(xpad360_absbit) / sizeof(xpad360_absbit[0]));
	
	for (int i = 0; i < size; ++i) {
		const int axis =  xpad360_absbit[i];
		const int min = xpad360_abs_params[i].min;
		const int max = xpad360_abs_params[i].max;
		const int fuzz = xpad360_abs_params[i].fuzz;
		const int flat = xpad360_abs_params[i].flat;
		
		input_set_abs_params(input_dev, axis, min, max, fuzz, flat);
	}
}

struct xpad360_controller {
	struct xpad360_transfer in;
	struct xpad360_transfer led_out;
	struct xpad360_transfer rumble_out;
	struct input_dev *input_dev;
};

static int xpad360_rumble(struct input_dev *dev, void* context, struct ff_effect *effect)
{
	struct xpad360_controller *controller = context;
	int status = -1;

	if (effect->type == FF_RUMBLE) {
		u8 left = effect->u.rumble.strong_magnitude / 255;
		u8 rite = effect->u.rumble.weak_magnitude / 255;
		
		const u8 packet[8] = { 
			0x00, 0x08, 0x00, 
			left, rite,
			0x00, 0x00, 0x00 
		};

		memcpy(controller->rumble_out.urb->transfer_buffer, packet, sizeof(packet));

		status = !!usb_submit_urb(controller->rumble_out.urb, GFP_ATOMIC);
	}
	
	return status;
}

inline
static void xpad360_gen_packet_led(u8 *buffer, enum xpad360_led led)
{
	const u8 packet[3] = { 0x01, 0x03, led };
	
	memcpy(buffer, packet, sizeof(packet));
}

inline static void xpad360_parse_input(struct input_dev *input_dev, u8 *data)
{
	/* D-pad */
	input_report_abs(input_dev, ABS_HAT0X, !!(data[0] & 0x08) - !!(data[0] & 0x04));
	input_report_abs(input_dev, ABS_HAT0Y, !!(data[0] & 0x02) - !!(data[0] & 0x01));
	
	/* start/back buttons */
	input_report_key(input_dev, BTN_START,  data[0] & 0x10);
	input_report_key(input_dev, BTN_SELECT, data[0] & 0x20); /* Back */

	/* stick press left/right */
	input_report_key(input_dev, BTN_THUMBL, data[0] & 0x40);
	input_report_key(input_dev, BTN_THUMBR, data[0] & 0x80);

	input_report_key(input_dev, BTN_TL, data[1] & 0x01); /* Left Shoulder */
	input_report_key(input_dev, BTN_TR, data[1] & 0x02); /* Right Shoulder */
	input_report_key(input_dev, BTN_MODE, data[1] & 0x04); /* Guide */
	/* data[8] & 0x08 is a dummy value */
	input_report_key(input_dev, BTN_A, data[1] & 0x10);
	input_report_key(input_dev, BTN_B, data[1] & 0x20);
	input_report_key(input_dev, BTN_X, data[1] & 0x40);
	input_report_key(input_dev, BTN_Y, data[1] & 0x80);

	input_report_abs(input_dev, ABS_Z, data[2]);
	input_report_abs(input_dev, ABS_RZ, data[3]);

	/* Left Stick */
	input_report_abs(input_dev, ABS_X, (__s16)le16_to_cpup((__le16*)&data[4]));
	input_report_abs(input_dev, ABS_Y, ~(__s16)le16_to_cpup((__le16*)&data[6]));

	/* Right Stick */
	input_report_abs(input_dev, ABS_RX, (__s16)le16_to_cpup((__le16*)&data[8]));
	input_report_abs(input_dev, ABS_RY, ~(__s16)le16_to_cpup((__le16*)&data[10]));
	
	input_sync(input_dev);
}

static void xpad360_send(struct urb *urb)
{
	/* We can't handle urb->status reasonably so just don't.  */
}

static void xpad360_receive(struct urb *urb)
{
	struct xpad360_controller *controller = urb->context;
	u8 *data = urb->transfer_buffer;
	
	switch (urb->status) {
	case 0: 
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	default:
		goto finish;
	}
	
	switch(le16_to_cpup((u16*)&data[0])) {
	case 0x0301: /* LED status */
		break;
	case 0x0303: /* Possibly a packet concerning rumble effect */
		break;
	case 0x0308: /* Attachment */
		break;
	case 0x1400:
		xpad360_parse_input(controller->input_dev, &data[2]);
	}
	
finish:
	usb_submit_urb(urb, GFP_ATOMIC);
}

static int xpad360_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int error = 0;
	struct usb_device *usb_dev = interface_to_usbdev(interface);
	struct xpad360_controller *controller = 
		kzalloc(sizeof(struct xpad360_controller), GFP_KERNEL);
		
	if (!controller)
		return -ENOMEM;
	
	error = xpad360_alloc_transfer(usb_dev, &controller->in, GFP_KERNEL);
	if (error) {
		goto fail_alloc_in;
	}
	
	usb_fill_int_urb(
		controller->in.urb, usb_dev,
		usb_rcvintpipe(usb_dev, 0x81),
		controller->in.buffer, 32,
		xpad360_receive, 
		controller, 4);
	
	controller->input_dev = input_allocate_device();
	if (!controller->input_dev) {
		error = -ENOMEM;
		goto fail_input_dev;
	}
	
	/* The LED and Rumble features don't have to succeed in setup for a usable controller.
	 * I treat both of them as optional */
	error = xpad360_alloc_transfer(usb_dev, &controller->led_out, GFP_KERNEL);
	if (error) 
		goto finish_led;
	
	xpad360_gen_packet_led(controller->led_out.buffer, XPAD360_LED_ON_1);
	
	usb_fill_int_urb(
		controller->led_out.urb, usb_dev,
		usb_sndintpipe(usb_dev, 0x01),
		controller->led_out.buffer, 32,
		xpad360_send, controller, 8);
	
	controller->led_out.urb->transfer_buffer_length = 3;
	
	error = usb_submit_urb(controller->led_out.urb, GFP_KERNEL);
	if (!error) /* If successful. */
		goto finish_led;
	
	xpad360_free_transfer(usb_dev, &controller->led_out);
	controller->led_out.urb = NULL;
	controller->led_out.buffer = NULL;
	
finish_led:
	error = xpad360_alloc_transfer(usb_dev, &controller->rumble_out, GFP_KERNEL);
	if (error)
		controller->rumble_out.urb = NULL;
		controller->rumble_out.buffer = NULL;
		goto required;
	
	usb_fill_int_urb(
		controller->rumble_out.urb, usb_dev,
		usb_sndintpipe(usb_dev, 0x01),
		controller->rumble_out.buffer, 32,
		xpad360_send, controller, 8);
	
	controller->rumble_out.urb->transfer_buffer_length = 8;
	
required:
	usb_to_input_id(usb_dev, &controller->input_dev->id);
	
	controller->input_dev->name = xpad360_device_names[id - xpad360_table];
	
	xpad360_set_keybit(controller->input_dev, xpad360_keybit, sizeof(xpad360_keybit) /  sizeof(xpad360_keybit[0]));
	xpad360_set_absbit(controller->input_dev, xpad360_absbit, sizeof(xpad360_absbit) / sizeof(xpad360_absbit[0]));
	xpad360_set_abs_params(controller->input_dev);
	
	error = input_ff_create_memless(controller->input_dev, controller, xpad360_rumble);
	if (!error) /* If successful. */
		xpad360_set_ffbit(controller->input_dev);

	error = input_register_device(controller->input_dev);
	if (error)
		goto fail_input_register;
	
	error = usb_submit_urb(controller->in.urb, GFP_KERNEL);
	if (error)
		goto fail_submit_in;
	
	usb_set_intfdata(interface, controller);
	
	goto success;

fail_submit_in:
	input_unregister_device(controller->input_dev);
	goto fail_input_dev;
fail_input_register:
	input_free_device(controller->input_dev);
fail_input_dev:
	xpad360_free_transfer(usb_dev, &controller->in);
fail_alloc_in:
	kfree(controller);
success:
	return error;
}

static void xpad360_disconnect(struct usb_interface *interface)
{
	struct usb_device *usb_dev = interface_to_usbdev(interface);
	struct xpad360_controller *controller = usb_get_intfdata(interface);
	
	xpad360_free_transfer(usb_dev, &controller->in);
	xpad360_free_transfer(usb_dev, &controller->rumble_out);
	xpad360_free_transfer(usb_dev, &controller->led_out);
	input_unregister_device(controller->input_dev);
	kfree(controller);
}

static struct usb_driver xpad360_driver = {
	.name		= "xpad360",
	.probe		= xpad360_probe,
	.disconnect	= xpad360_disconnect,
	.id_table	= xpad360_table,
	/* .soft_unbind	= 1 *//* Allows us to set LED properly before module unload. */
};

MODULE_DEVICE_TABLE(usb, xpad360_table);
module_usb_driver(xpad360_driver);
