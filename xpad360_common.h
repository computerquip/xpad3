#pragma once

enum xpad360_led {
	XPAD360_LED_OFF,
	XPAD360_LED_ALL_BLINKING,
	XPAD360_LED_FLASH_ON_1,
	XPAD360_LED_FLASH_ON_2,
	XPAD360_LED_FLASH_ON_3,
	XPAD360_LED_FLASH_ON_4,
	XPAD360_LED_ON_1,
	XPAD360_LED_ON_2,
	XPAD360_LED_ON_3,
	XPAD360_LED_ON_4,
	XPAD360_LED_ROTATING,
	XPAD360_LED_SECTIONAL_BLINKING,
	XPAD360_LED_SLOW_SECTIONAL_BLINKING,
	XPAD360_LED_ALTERNATING
};

struct xpad360_transfer {
	struct urb *urb;
	u8 *buffer;
	dma_addr_t dma;
};

inline 
static void xpad360_set_absbit(struct input_dev *input_dev, const int *types, size_t size)
{
	__set_bit(EV_ABS, input_dev->evbit);
	
	for (int i = 0; i < size; ++i) {
		__set_bit(types[i], input_dev->absbit);
	}
}

inline 
static void xpad360_set_keybit(struct input_dev *input_dev, const int *types, size_t size)
{
	__set_bit(EV_KEY, input_dev->evbit);
	
	for (int i = 0; i < size; ++i) {
		__set_bit(types[i], input_dev->keybit);
	}
}

inline 
static void xpad360_set_ffbit(struct input_dev *input_dev)
{
	/* Xbox controllers only support basic rumble effect. */
	__set_bit(EV_FF, input_dev->evbit);
	__set_bit(FF_RUMBLE, input_dev->ffbit);
}

inline 
static int xpad360_alloc_transfer(struct usb_device *usb_dev, struct xpad360_transfer *transfer, gfp_t mem_flag)
{
	transfer->urb = usb_alloc_urb(0, mem_flag);
	if (!transfer->urb) {
		return -ENOMEM;
	}
	
	transfer->buffer = usb_alloc_coherent(usb_dev, 32, mem_flag, &transfer->dma);
	if (!transfer->buffer) {
		usb_free_urb(transfer->urb);
		return -ENOMEM;
	}
	
	return 0;
}

inline
static void xpad360_free_transfer(struct usb_device *usb_dev, struct xpad360_transfer *transfer)
{
	usb_kill_urb(transfer->urb);
	usb_free_urb(transfer->urb);
	usb_free_coherent(usb_dev, 32, transfer->buffer, transfer->dma);
}
