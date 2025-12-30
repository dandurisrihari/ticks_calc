#!/bin/bash
# unload_module.sh - Unload kernel modules

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo $0)"
    exit 1
fi

unload_module() {
    local name="$1"

    if ! lsmod | grep -q "^$name"; then
        echo "$name: not loaded"
        return 0
    fi

    echo "Unloading $name..."
    if rmmod "$name"; then
        echo "$name: unloaded"
    else
        echo "$name: failed to unload"
        return 1
    fi
}

# Unload both modules
unload_module "int_latency_driver"
unload_module "pagewalk_driver"

echo ""
echo "Kernel messages:"
dmesg | tail -5
