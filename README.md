# Timing Measurement Drivers for i.MX8MP

Kernel drivers for precise timing measurements on NXP i.MX8M Plus using the ARM64 cycle counter (PMCCNTR_EL0).

## Features

- **Page Table Walk Timing** - Measure time to walk the Linux page tables
- **Software Interrupt Latency** - Measure hrtimer interrupt delivery latency
- **Cycle-accurate measurements** - Uses ARM64 Performance Monitor cycle counter
- **Cross-compilation ready** - For NXP i.MX8MP EVK

## Project Structure

```
ticks_calc/
├── kernel/
│   ├── pagewalk_driver.c      # Page table walk timing driver
│   ├── int_latency_driver.c   # Software interrupt latency driver
│   └── Makefile
├── userspace/
│   ├── pagewalk_test.c        # Pagewalk test application
│   ├── int_latency_test.c     # Interrupt latency test application
│   └── Makefile
├── include/
│   ├── pagewalk_ioctl.h       # Pagewalk IOCTL definitions
│   └── int_latency_ioctl.h    # Interrupt latency IOCTL definitions
├── scripts/
│   ├── load_module.sh         # Load kernel modules
│   ├── unload_module.sh       # Unload kernel modules
│   └── run_tests.sh           # Run test suite
├── Makefile                   # Top-level build
└── README.md
```

## Prerequisites

### Target Hardware
- NXP i.MX8M Plus EVK (or compatible)
- Linux kernel with PMU access enabled

### Host Build Environment
- NXP Yocto SDK for i.MX8MP
- Cross-compiler: `aarch64-poky-linux-gcc`
- Kernel headers from linux-imx

## Setting Up the Toolchain

### 1. Source the Yocto SDK Environment

Before building, source the SDK environment script:

```bash
source /opt/fsl-imx-fb/6.6-scarthgap/environment-setup-armv8a-poky-linux
```

This sets up:
- `CC` - Cross compiler
- `CROSS_COMPILE` - Compiler prefix
- `SDKTARGETSYSROOT` - Sysroot path
- Other necessary environment variables

### 2. Verify Toolchain

```bash
# Check compiler
aarch64-poky-linux-gcc --version

# Check sysroot
echo $SDKTARGETSYSROOT
# Should show: /opt/fsl-imx-fb/6.6-scarthgap/sysroots/armv8a-poky-linux
```

### 3. Configure Kernel Path

Edit the top-level `Makefile` to point to your kernel source:

```makefile
KDIR := /path/to/your/linux-imx
```

Default: `/media/sri/D/Research/Accelerators_Research/nxp_8mplusbb/linux-imx`

## Building

### Build Everything

```bash
make
```

### Build Individual Components

```bash
make kernel      # Build kernel modules only
make userspace   # Build userspace apps only
make clean       # Clean all build artifacts
```

### Build Output

After successful build:
- `kernel/pagewalk_driver.ko` - Page walk timing module
- `kernel/int_latency_driver.ko` - Interrupt latency module
- `userspace/pagewalk_test` - Page walk test app (ARM64)
- `userspace/int_latency_test` - Interrupt latency test app (ARM64)

## Deployment to Target

### Copy to Target

```bash
scp -r ticks_calc/ root@<target_ip>:~/
```

### On the Target

```bash
cd ~/ticks_calc

# Load modules
insmod kernel/pagewalk_driver.ko
insmod kernel/int_latency_driver.ko

# Verify devices
ls -la /dev/pagewalk_timer /dev/int_latency
```

## Usage

### Page Table Walk Timing

Measures the time to walk Linux page tables for virtual-to-physical address translation.

```bash
# Single measurement
./userspace/pagewalk_test

# Average over N samples
./userspace/pagewalk_test 100
```

**Sample Output:**
```
Pagewalk Timing Test
====================
PID: 1234, Page size: 4096 bytes, Samples: 100

=== Stack Variable ===
  Virtual Address:  0x0000fffff7fff000
  Physical Address: 0x00000001325ff000
  Status:           VALID
  Huge Page:        No (4KB)
  Samples:          100
  Avg Walk Time:    95 cycles, 125 ns
  Min Walk Time:    89 cycles, 125 ns
  Max Walk Time:    402 cycles, 250 ns
```

### Software Interrupt Latency

Measures hrtimer interrupt delivery latency (time from timer trigger to handler execution).

```bash
# 10 samples (default)
./userspace/int_latency_test

# N samples
./userspace/int_latency_test 100
```

**Sample Output:**
```
Interrupt Latency Test (10 samples)
====================================
    #        Cycles   Nanoseconds
    1          5773          3372 ns
    2          5609          3122 ns
    3          5813          3372 ns
...
------------------------------------
Avg:           5732          3289 ns
Min:                           3122 ns
Max:                           3872 ns
```

## Technical Details

### ARM64 Cycle Counter

Both drivers use the ARM64 Performance Monitor cycle counter (`PMCCNTR_EL0`) for precise timing:

```c
/* Enable cycle counter */
asm volatile("mrs %0, pmcntenset_el0" : "=r"(val));
asm volatile("msr pmcntenset_el0, %0" : : "r"(val | (1UL << 31)));

/* Read cycle counter */
isb();
asm volatile("mrs %0, pmccntr_el0" : "=r"(val));
```

### Page Walk Implementation

The pagewalk driver traverses the 4-level page table hierarchy:
1. **PGD** - Page Global Directory
2. **P4D** - Page 4th Directory (pass-through on ARM64 4-level paging)
3. **PUD** - Page Upper Directory
4. **PMD** - Page Middle Directory (checks for 2MB huge pages)
5. **PTE** - Page Table Entry (4KB pages)

### Interrupt Latency Measurement

Uses Linux high-resolution timers (`hrtimer`):
1. Records timestamp before starting timer
2. Timer fires immediately (1ns minimum)
3. Handler records arrival timestamp
4. Difference = interrupt delivery latency

## Typical Results (i.MX8MP @ 1.8GHz)

| Measurement | Typical Value |
|-------------|---------------|
| Page walk (cached) | 90-150 cycles (50-100 ns) |
| Page walk (first access) | 300-500 cycles (150-300 ns) |
| Interrupt latency | 5000-8000 cycles (3-5 µs) |

## Troubleshooting

### Module Load Fails

```bash
# Check kernel messages
dmesg | tail -20

# Verify module format
file kernel/pagewalk_driver.ko
# Should show: ELF 64-bit LSB relocatable, ARM aarch64
```

### Device Not Created

```bash
# Check if module is loaded
lsmod | grep pagewalk
lsmod | grep int_latency

# Check kernel log for errors
dmesg | grep -E "pagewalk|int_latency"
```

### Permission Denied

```bash
# Devices should be world-accessible (mode 0666)
ls -la /dev/pagewalk_timer /dev/int_latency

# If not, fix permissions
chmod 666 /dev/pagewalk_timer /dev/int_latency
```

### Cross-Compilation Errors

```bash
# Ensure SDK is sourced
source /opt/fsl-imx-fb/6.6-scarthgap/environment-setup-armv8a-poky-linux

# Verify KDIR points to correct kernel source
echo $KDIR

# Check kernel is built
ls $KDIR/Module.symvers
```

## License

GPL-2.0
