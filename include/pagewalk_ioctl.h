/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pagewalk_ioctl.h - Shared header for kernel driver and userspace
 *
 * Defines ioctl commands and data structures for pagewalk timing measurement
 */

#ifndef _PAGEWALK_IOCTL_H
#define _PAGEWALK_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>

typedef uint64_t __u64;
typedef uint32_t __u32;
typedef int32_t __s32;
#endif

/* Magic number for ioctl commands */
#define PAGEWALK_IOC_MAGIC  'P'

/* Address validity result codes */
#define ADDR_VALID          0
#define ADDR_INVALID_PGD    1
#define ADDR_INVALID_P4D    2
#define ADDR_INVALID_PUD    3
#define ADDR_INVALID_PMD    4
#define ADDR_INVALID_PTE    5
#define ADDR_NOT_PRESENT    6
#define ADDR_PROCESS_NOT_FOUND  7
#define ADDR_NO_MM          8

/* Structure for address check request */
struct pagewalk_request {
    __s32 pid;              /* Target process PID */
    __u64 vaddr;            /* Virtual address to check */
};

/* Structure for timing results (all times in nanoseconds) */
struct pagewalk_result {
    __u64 vaddr;            /* Virtual address that was checked */
    __u64 paddr;            /* Physical address (if valid) */
    __u32 valid;            /* Validity result code (see ADDR_* defines) */
    __u32 reserved;         /* Padding for alignment */
    __u64 pagewalk_time_ns; /* Time taken for pagewalk in nanoseconds */
};

/* Structure for interrupt latency measurement */
struct interrupt_timing {
    __u64 trigger_time_ns;      /* Time when interrupt was triggered */
    __u64 handler_entry_ns;     /* Time when handler started */
    __u64 pagewalk_start_ns;    /* Time when pagewalk started */
    __u64 pagewalk_end_ns;      /* Time when pagewalk completed */
    __u64 handler_exit_ns;      /* Time when handler completed */
    __u64 interrupt_latency_ns; /* handler_entry - trigger_time */
    __u64 total_latency_ns;     /* handler_exit - trigger_time */
    __u32 valid;                /* Address validity result */
    __u32 reserved;             /* Padding */
};

/* Statistics structure for multiple measurements */
struct pagewalk_stats {
    __u64 min_pagewalk_ns;
    __u64 max_pagewalk_ns;
    __u64 avg_pagewalk_ns;
    __u64 min_irq_latency_ns;
    __u64 max_irq_latency_ns;
    __u64 avg_irq_latency_ns;
    __u32 sample_count;
    __u32 reserved;
};

/*
 * IOCTL Commands
 */

/* Set target process PID */
#define PAGEWALK_SET_PID        _IOW(PAGEWALK_IOC_MAGIC, 1, __s32)

/* Set target virtual address */
#define PAGEWALK_SET_ADDR       _IOW(PAGEWALK_IOC_MAGIC, 2, __u64)

/* Perform pagewalk and get timing (synchronous, no interrupt) */
#define PAGEWALK_CHECK          _IOWR(PAGEWALK_IOC_MAGIC, 3, struct pagewalk_result)

/* Trigger interrupt-based measurement */
#define PAGEWALK_TRIGGER_IRQ    _IO(PAGEWALK_IOC_MAGIC, 4)

/* Get interrupt timing results */
#define PAGEWALK_GET_IRQ_TIMING _IOR(PAGEWALK_IOC_MAGIC, 5, struct interrupt_timing)

/* Run multiple samples and get statistics */
#define PAGEWALK_RUN_STATS      _IOWR(PAGEWALK_IOC_MAGIC, 6, struct pagewalk_stats)

/* Full request with PID and address in one call */
#define PAGEWALK_CHECK_FULL     _IOWR(PAGEWALK_IOC_MAGIC, 7, struct pagewalk_request)

/* Device name */
#define PAGEWALK_DEVICE_NAME    "pagewalk_timer"
#define PAGEWALK_DEVICE_PATH    "/dev/pagewalk_timer"

#endif /* _PAGEWALK_IOCTL_H */
