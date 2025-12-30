# SPDX-License-Identifier: GPL-2.0
#
# Makefile for kernel modules
#
# For cross-compilation (i.MX8MP), set:
#   CROSS_COMPILE=aarch64-linux-gnu-
#   KDIR=/path/to/linux-imx
#
# Example: make CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/linux-imx
#

obj-m := pagewalk_driver.o int_latency_driver.o

# Kernel build directory - override with KDIR=/path/to/kernel
# For cross-compilation, pass KDIR from top-level Makefile
KDIR ?= /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Cross compiler prefix (empty for native build)
# For i.MX8MP: CROSS_COMPILE=aarch64-poky-linux-
CROSS_COMPILE ?=

# Architecture (arm64 for i.MX8MP, auto-detect for native)
ARCH ?= arm64

# Extra compiler flags
ccflags-y := -I$(PWD)/../include

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || rm -f *.o *.ko *.mod* .*.cmd Module.symvers modules.order

# Install module
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# Load module
load:
	insmod pagewalk_driver.ko

# Unload module
unload:
	rmmod pagewalk_driver

# Reload module
reload: unload load

.PHONY: all clean install load unload reload
