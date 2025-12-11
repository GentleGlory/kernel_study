obj-m := virtual_video.o
virtual_video-objs := virtual_video_drv.o red.o green.o blue.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

rpi3b:
	$(MAKE) -C ../../../linux M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean
	$(MAKE) -C ../../../linux M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-


.PHONY: default clean install
