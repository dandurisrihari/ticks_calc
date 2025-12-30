#!/bin/bash
# unload_module.sh - Unload the pagewalk_driver kernel module

MODULE_NAME="pagewalk_driver"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo $0)"
    exit 1
fi

# Check if module is loaded
if ! lsmod | grep -q "$MODULE_NAME"; then
    echo "Module $MODULE_NAME is not loaded"
    exit 0
fi

# Unload the module
echo "Unloading $MODULE_NAME..."
rmmod "$MODULE_NAME"

if [ $? -eq 0 ]; then
    echo "Module unloaded successfully"
    
    # Show dmesg output
    echo ""
    echo "Kernel messages:"
    dmesg | tail -3
else
    echo "Failed to unload module"
    echo "Make sure no processes are using /dev/pagewalk_timer"
    dmesg | tail -5
    exit 1
fi
