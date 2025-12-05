obj-m += pci-host-test.o

BUILD ?= ../../build
KDIR ?= $(BUILD)/linux
PWD := $(shell pwd)
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-
DTB := frank_pcie_test.dtb
DTS := frank_pcie_test.dts


dtb: $(DTS)
	dtc -I dts -O dtb -o $(DTB) $(DTS)

all: dtb
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules	
	cp $(DTB) $(BUILD)
	cp *.ko $(BUILD)

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
	rm -f $(DTB)