#!/bin/bash
# load_module.sh - Load kernel modules

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/../kernel"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo $0)"
    exit 1
fi

load_module() {
    local name="$1"
    local path="$KERNEL_DIR/${name}.ko"

    if [ ! -f "$path" ]; then
        echo "Warning: $path not found"
        return 1
    fi

    if lsmod | grep -q "^$name"; then
        echo "$name: already loaded"
        return 0
    fi

    echo "Loading $name..."
    if insmod "$path"; then
        echo "$name: loaded successfully"
    else
        echo "$name: failed to load"
        return 1
    fi
}

# Load both modules
load_module "pagewalk_driver"
load_module "int_latency_driver"

echo ""
echo "Devices:"
ls -la /dev/pagewalk_timer /dev/int_latency 2>/dev/null || echo "No devices found"

echo ""
echo "Kernel messages:"
dmesg | tail -5
