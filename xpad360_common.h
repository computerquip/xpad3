#pragma once

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
static void xpad360_set_ffbit(struct input_dev *input_dev, const int *types, size_t size)
{
	__set_bit(EV_FF, input_dev->evbit);
	
	for (int i = 0; i < size; ++i) {
		__set_bit(types[i], input_dev->ffbit);
	}
}
