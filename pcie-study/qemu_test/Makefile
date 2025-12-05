# ===== User knobs =====
ARCH ?= arm64
CROSS ?= aarch64-linux-gnu-
J ?= $(shell nproc)
KVER ?= v6.16
BBVER ?= 1.36.1

# ===== Layout =====
ROOT := $(CURDIR)
BUILD := $(ROOT)/build
KDIR := $(BUILD)/linux
BBFILE := busybox-$(BBVER).tar.bz2
BBTAR := $(BUILD)/$(BBFILE)
BBDIR := $(BUILD)/busybox-$(BBVER)
IMAGE := $(KDIR)/arch/$(ARCH)/boot/Image
DTB := $(BUILD)/frank_pcie_test.dtb
INITRD := $(BUILD)/rootfs.cpio.gz
SRC := $(ROOT)/src
SRC_PCIE_DRV := $(SRC)/frank_pcie
SRC_PCIE_DRV_FILE := pci-host-test.ko
SRC_E1000E_DRV := $(SRC)/frank_e1000e
SRC_E1000E_DRV_FILE := frank_e1000e.ko

.PHONY: all run kernel busybox rootfs menuconfig kdefconfig kconfig-min clean kfrag

all: run

$(BUILD):
	mkdir -p $@

# ---------- Kernel ----------
$(KDIR): | $(BUILD)
	git clone --depth=1 --branch $(KVER) https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git $(KDIR)

kdefconfig: $(KDIR)
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) defconfig

# Minimal config fragment merge (enables PCI host generic & e1000e, etc.)
kconfig-min: kdefconfig kfrag
	$(KDIR)/scripts/kconfig/merge_config.sh -m -O $(KDIR) $(KDIR)/.config $(ROOT)/kfrag
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) olddefconfig

kernel: kconfig-min
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -j$(J)
		
# Compile our custom DTB and pcie driver
$(SRC_PCIE_DRV_FILE): kernel
	$(MAKE) -C $(SRC_PCIE_DRV) clean
	$(MAKE) -C $(SRC_PCIE_DRV) all


# Compile our custom e1000e module
$(SRC_E1000E_DRV_FILE): kernel
	$(MAKE) -C $(SRC_E1000E_DRV) clean
	$(MAKE) -C $(SRC_E1000E_DRV) all

menuconfig:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) menuconfig

# ---------- BusyBox ----------
$(BBTAR): | $(BUILD)
	cp $(SRC)/$(BBFILE) $(BBTAR)

$(BBDIR): $(BBTAR)
	tar xjf $< -C $(BUILD)

busybox: $(BBDIR)
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) defconfig
	# force static busybox to avoid shared libs in initramfs
	sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' $(BBDIR)/.config
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -j$(J)
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) install

# ---------- RootFS (initramfs) ----------
rootfs: busybox
	rm -rf $(BUILD)/rootfs && mkdir -p $(BUILD)/rootfs
	cp -a $(BBDIR)/_install/* $(BUILD)/rootfs/
	mkdir -p $(BUILD)/rootfs/usr/lib/modules
	cp -a $(BUILD)/*.ko $(BUILD)/rootfs/usr/lib/modules
	cp -a $(BUILD)/linux/drivers/net/ethernet/intel/e1000e/*.ko $(BUILD)/rootfs/usr/lib/modules
	cp -a $(BUILD)/linux/drivers/net/ethernet/intel/e1000/*.ko $(BUILD)/rootfs/usr/lib/modules
	mkdir -p $(BUILD)/rootfs/proc $(BUILD)/rootfs/sys $(BUILD)/rootfs/dev $(BUILD)/rootfs/tmp $(BUILD)/rootfs/root $(BUILD)/rootfs/home
	printf '%s\n' '#!/bin/sh' \
	'mount -t proc none /proc' \
	'mount -t sysfs none /sys' \
	'mount -t tmpfs none /tmp' \
	'insmod /usr/lib/modules/$(SRC_PCIE_DRV_FILE)' \
	'insmod /usr/lib/modules/$(SRC_E1000E_DRV_FILE)' \
	'echo "Hello from minimal rootfs!"' \
	'exec /bin/sh' > $(BUILD)/rootfs/init
	chmod +x $(BUILD)/rootfs/init
	( cd $(BUILD)/rootfs && find . | cpio -H newc -o ) | gzip -9 > $(INITRD)

# ---------- Run ----------
run: kernel $(SRC_PCIE_DRV_FILE) $(SRC_E1000E_DRV_FILE) rootfs scripts/run_qemu.sh
	bash scripts/run_qemu.sh "$(IMAGE)" "$(DTB)" "$(INITRD)"

clean:
	[ -d "$(KDIR)" ] && $(MAKE) -C $(SRC_PCIE_DRV) clean || true
	[ -d "$(KDIR)" ] && $(MAKE) -C $(SRC_E1000E_DRV) clean || true
	[ -d "$(KDIR)" ] && $(MAKE) -C $(KDIR) ARCH=$(ARCH) clean || true
	[ -d "$(BBDIR)" ] && $(MAKE) -C $(BBDIR) clean || true
	rm -f $(INITRD)
	rm -f kfrag

distclean:
	rm -f kfrag
	rm -rf $(BUILD)

# ---------- Files ----------
kfrag:
	@echo 'CONFIG_PRINTK_TIME=y' > kfrag
	@echo 'CONFIG_PCI=y' >> kfrag
	@echo 'CONFIG_PCI_MSI=y' >> kfrag
	@echo 'CONFIG_DEVTMPFS=y' >> kfrag
	@echo 'CONFIG_DEVTMPFS_MOUNT=y' >> kfrag
	@echo 'CONFIG_TMPFS=y' >> kfrag
	@echo 'CONFIG_BLK_DEV_INITRD=y' >> kfrag
	@echo 'CONFIG_E1000=m' >> kfrag
	@echo 'CONFIG_E1000E=m' >> kfrag
	@echo '# helpful but optional:' >> kfrag
	@echo 'CONFIG_VIRTIO_BLK=y' >> kfrag
	

# Ensure helper scripts exist
scripts/run_qemu.sh:
	mkdir -p scripts
	@echo '#!/usr/bin/env bash' > $@
	@echo 'set -euo pipefail' >> $@
	@echo 'IMAGE="$${1:-}"; DTB="$${2:-}"; INITRD="$${3:-}"' >> $@
	@echo 'if [[ -z "$$IMAGE" || -z "$$DTB" || -z "$$INITRD" ]]; then' >> $@
	@echo '  echo "usage: $$0 <Image> <dtb> <rootfs.cpio.gz>" >&2; exit 1' >> $@
	@echo 'fi' >> $@
	@echo 'qemu-system-aarch64 \' >> $@
	@echo '  -M virt \' >> $@
	@echo '  -cpu cortex-a57 \' >> $@
	@echo '  -smp 2 -m 1024 \' >> $@
	@echo '  -kernel "$$IMAGE" \' >> $@
	@echo '  -initrd "$$INITRD" \' >> $@
	@echo '  -append "console=ttyAMA0" \' >> $@
	@echo '  -device e1000e,netdev=net0 \' >> $@
	@echo '  -netdev user,id=net0 \' >> $@
	@echo '  -nographic \' >> $@
	@echo '  -dtb "$$DTB"'  >> $@
	chmod +x $@

