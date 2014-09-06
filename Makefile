obj-m := xpad360.o xpad360wr.o

xpad360-y  := xpad360_usb.o
xpad360wr-y := xpad360wr_usb.o

ccflags-y   := -DDEBUG -std=gnu99

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
