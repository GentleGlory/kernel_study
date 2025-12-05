obj-m += frank_e1000e.o
frank_e1000e-objs := frank_e1000e_main.o

BUILD ?= ../../build
KDIR ?= $(BUILD)/linux
ROOTFS_MODULES ?= $(BUILD)/rootfs/usr/lib/modules
PWD := $(shell pwd)
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
ROOTFS_MODULES ?= $(BUILD)/rootfs/usr/lib/modules

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	cp *.ko $(BUILD)

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean