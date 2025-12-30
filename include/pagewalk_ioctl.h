/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pagewalk_ioctl.h - Shared header for pagewalk timing driver
 *
 * Simple interface: send virtual address, get validity and precise timing
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

/* Device name */
#define PAGEWALK_DEVICE_NAME "pagewalk_timer"

/* Address validity result codes */
#define ADDR_VALID              0
#define ADDR_INVALID_PGD        1
#define ADDR_INVALID_P4D        2
#define ADDR_INVALID_PUD        3
#define ADDR_INVALID_PMD        4
#define ADDR_INVALID_PTE        5
#define ADDR_NOT_PRESENT        6
#define ADDR_PROCESS_NOT_FOUND  7
#define ADDR_NO_MM              8

/*
 * Pagewalk request/result structure
 * Send PID + virtual address, get physical address + validity + timing
 */
struct pagewalk_request {
    /* Input */
    __s32 pid;                  /* Target process PID (use getpid() for self) */
    __u32 reserved;             /* Padding */
    __u64 vaddr;                /* Virtual address to check */
    
    /* Output */
    __u64 paddr;                /* Physical address (if valid) */
    __u32 valid;                /* Validity result (ADDR_* codes) */
    __u32 is_huge_page;         /* 1 if huge page (2MB), 0 otherwise */
    
    /* Precise timing using ARM64 cycle counter */
    __u64 walk_cycles;          /* Page table walk time in CPU cycles */
    __u64 walk_ns;              /* Page table walk time in nanoseconds (backup) */
};

/*
 * IOCTL Command - single call does everything
 */
#define PAGEWALK_CHECK  _IOWR(PAGEWALK_IOC_MAGIC, 1, struct pagewalk_request)

#endif /* _PAGEWALK_IOCTL_H */
