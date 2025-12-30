#!/bin/bash
# load_module.sh - Load the pagewalk_driver kernel module

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_PATH="$SCRIPT_DIR/../kernel/pagewalk_driver.ko"
MODULE_NAME="pagewalk_driver"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo $0)"
    exit 1
fi

# Check if module file exists
if [ ! -f "$MODULE_PATH" ]; then
    echo "Error: Module not found at $MODULE_PATH"
    echo "Run 'make' in the project root first."
    exit 1
fi

# Check if already loaded
if lsmod | grep -q "$MODULE_NAME"; then
    echo "Module $MODULE_NAME is already loaded"
    exit 0
fi

# Load the module
echo "Loading $MODULE_NAME..."
insmod "$MODULE_PATH"

if [ $? -eq 0 ]; then
    echo "Module loaded successfully"
    
    # Show device info
    if [ -e /dev/pagewalk_timer ]; then
        echo "Device created: /dev/pagewalk_timer"
        ls -la /dev/pagewalk_timer
    else
        echo "Warning: Device /dev/pagewalk_timer not found"
    fi
    
    # Show dmesg output
    echo ""
    echo "Kernel messages:"
    dmesg | tail -5
else
    echo "Failed to load module"
    dmesg | tail -5
    exit 1
fi
