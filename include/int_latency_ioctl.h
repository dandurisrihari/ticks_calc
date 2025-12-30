/* SPDX-License-Identifier: GPL-2.0 */
/*
 * int_latency_ioctl.h - Software interrupt latency measurement
 */

#ifndef _INT_LATENCY_IOCTL_H
#define _INT_LATENCY_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint64_t __u64;
typedef uint32_t __u32;
#endif

#define INT_LATENCY_MAGIC       'I'
#define INT_LATENCY_DEVICE_NAME "int_latency"

struct int_latency_result {
    __u64 latency_cycles;   /* Interrupt latency in CPU cycles */
    __u64 latency_ns;       /* Interrupt latency in nanoseconds */
    __u32 valid;            /* 1 if measurement succeeded */
    __u32 reserved;         /* Padding for alignment */
};

/* Measure interrupt latency */
#define INT_LATENCY_MEASURE _IOR(INT_LATENCY_MAGIC, 1, struct int_latency_result)

#endif /* _INT_LATENCY_IOCTL_H */
