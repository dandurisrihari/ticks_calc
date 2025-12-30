# Top-level Makefile for Pagewalk Timing Project
#
# This builds both the kernel module and userspace application

# Cross-compilation settings for i.MX8MP
KDIR := /media/sri/D/Research/Accelerators_Research/nxp_8mplusbb/linux-imx
CROSS_COMPILE := aarch64-poky-linux-
ARCH := arm64
CC := $(CROSS_COMPILE)gcc

# Export variables to sub-makes
export KDIR CROSS_COMPILE ARCH CC

.PHONY: all kernel userspace clean help install load unload test

# Default target: build everything
all: kernel userspace

# Build kernel module
kernel:
	$(MAKE) -C kernel KDIR=$(KDIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH)

# Build userspace application
userspace:
	$(MAKE) -C userspace CC=$(CC)

# Clean everything
clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C userspace clean

# Install kernel module (requires root)
install:
	$(MAKE) -C kernel install

# Load kernel module (requires root)
load:
	@if lsmod | grep -q pagewalk_driver; then \
		echo "Module already loaded"; \
	else \
		insmod kernel/pagewalk_driver.ko; \
		echo "Module loaded"; \
	fi
	@ls -la /dev/pagewalk_timer 2>/dev/null || echo "Device not found"

# Unload kernel module (requires root)
unload:
	@if lsmod | grep -q pagewalk_driver; then \
		rmmod pagewalk_driver; \
		echo "Module unloaded"; \
	else \
		echo "Module not loaded"; \
	fi

# Reload module
reload: unload load

# Run test application
test: all
	@if [ ! -e /dev/pagewalk_timer ]; then \
		echo "Error: Module not loaded. Run 'sudo make load' first."; \
		exit 1; \
	fi
	./userspace/pagewalk_test

# Show help
help:
	@echo "Timing Measurement Project (i.MX8MP)"
	@echo "===================================="
	@echo ""
	@echo "Build targets:"
	@echo "  make all       - Build kernel modules and userspace apps"
	@echo "  make kernel    - Build kernel modules only"
	@echo "  make userspace - Build userspace apps only"
	@echo "  make clean     - Clean all build artifacts"
	@echo ""
	@echo "Modules:"
	@echo "  pagewalk_driver.ko    - Page table walk timing"
	@echo "  int_latency_driver.ko - Software interrupt latency"
	@echo ""
	@echo "Typical workflow:"
	@echo "  1. make                    # Build on host"
	@echo "  2. scp -r ticks_calc/ root@<target>:~/"
	@echo "  3. insmod pagewalk_driver.ko"
	@echo "  4. insmod int_latency_driver.ko"
	@echo "  5. ./pagewalk_test"
	@echo "  6. ./int_latency_test [count]"
