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
	@echo "Pagewalk Timing Project"
	@echo "======================="
	@echo ""
	@echo "Build targets:"
	@echo "  make all       - Build kernel module and userspace app"
	@echo "  make kernel    - Build kernel module only"
	@echo "  make userspace - Build userspace app only"
	@echo "  make clean     - Clean all build artifacts"
	@echo ""
	@echo "Module management (requires root on target):"
	@echo "  make load      - Load kernel module"
	@echo "  make unload    - Unload kernel module"
	@echo "  make reload    - Reload kernel module"
	@echo ""
	@echo "Testing (on target):"
	@echo "  make test      - Run pagewalk test"
	@echo ""
	@echo "Typical workflow:"
	@echo "  1. make kernel    (build on host)"
	@echo "  2. Copy pagewalk_driver.ko and pagewalk_test to target"
	@echo "  3. insmod pagewalk_driver.ko"
	@echo "  4. ./pagewalk_test [optional_address_hex]"
