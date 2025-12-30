# SPDX-License-Identifier: GPL-2.0
#
# Makefile for pagewalk_driver kernel module
#
# For cross-compilation (i.MX8MP), set:
#   CROSS_COMPILE=aarch64-linux-gnu-
#   KDIR=/path/to/linux-imx
#
# Example: make CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/linux-imx
#

obj-m := pagewalk_driver.o

# Kernel build directory - override with KDIR=/path/to/kernel
KDIR ?= /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Cross compiler prefix (empty for native build)
CROSS_COMPILE ?=

# Architecture (auto-detect or override)
ARCH ?= $(shell uname -m | sed 's/aarch64/arm64/' | sed 's/x86_64/x86/')

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
