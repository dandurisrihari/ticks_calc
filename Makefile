# Top-level Makefile for Pagewalk Timing Project
#
# This builds both the kernel module and userspace application

KDIR := /media/sri/D/Research/Accelerators_Research/nxp_8mplusbb/linux-imx

CROSS_COMPILE := aarch64-poky-linux-

.PHONY: all kernel userspace clean help install load unload test

# Default target: build everything
all: kernel userspace

# Build kernel module
kernel:
	$(MAKE) -C kernel

# Build userspace application
userspace:
	$(MAKE) -C userspace

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
	./userspace/pagewalk_test -A

# Quick test (sync pagewalk only)
quicktest: all
	@if [ ! -e /dev/pagewalk_timer ]; then \
		echo "Error: Module not loaded. Run 'sudo make load' first."; \
		exit 1; \
	fi
	./userspace/pagewalk_test

# Run statistics with custom sample count
stats: all
	@if [ ! -e /dev/pagewalk_timer ]; then \
		echo "Error: Module not loaded. Run 'sudo make load' first."; \
		exit 1; \
	fi
	./userspace/pagewalk_test -s -n 1000

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
	@echo "Module management (requires root):"
	@echo "  make load      - Load kernel module"
	@echo "  make unload    - Unload kernel module"
	@echo "  make reload    - Reload kernel module"
	@echo "  make install   - Install module to system"
	@echo ""
	@echo "Testing:"
	@echo "  make test      - Run all tests"
	@echo "  make quicktest - Run quick sync pagewalk test"
	@echo "  make stats     - Run statistics with 1000 samples"
	@echo ""
	@echo "Typical workflow:"
	@echo "  1. make all"
	@echo "  2. sudo make load"
	@echo "  3. make test"
	@echo "  4. sudo make unload"
