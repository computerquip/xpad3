obj-m := xpad360.o xpad360c.o

xpad360-y  := xpad360_usb.o
xpad360c-y := xpad360_common.o

ccflags-y   := -DDEBUG -std=gnu99

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
