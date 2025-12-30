// SPDX-License-Identifier: GPL-2.0
/*
 * int_latency_driver.c - Software interrupt latency measurement for ARM64
 *
 * Measures hrtimer interrupt latency using ARM64 cycle counter (PMCCNTR_EL0).
 * Triggers a high-resolution timer and measures the time from trigger to handler.
 *
 * Usage:
 *   insmod int_latency_driver.ko
 *   ./int_latency_test [count]
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "../include/int_latency_ioctl.h"

#define DRIVER_NAME    "int_latency"
#define DRIVER_VERSION "2.0"

/* Measurement state */
static spinlock_t lock;
static volatile int meas_pending;
static volatile int meas_done;
static u64 trigger_cycles, trigger_ns;
static u64 handler_cycles, handler_ns;
static struct hrtimer timer;

/*
 * ARM64 cycle counter access
 */
static inline void enable_cycle_counter(void)
{
    u64 val;

    /* Enable cycle counter in PMCNTENSET */
    asm volatile("mrs %0, pmcntenset_el0" : "=r"(val));
    asm volatile("msr pmcntenset_el0, %0" : : "r"(val | (1UL << 31)));

    /* Enable counting: E bit, reset cycle counter: C bit */
    asm volatile("mrs %0, pmcr_el0" : "=r"(val));
    asm volatile("msr pmcr_el0, %0" : : "r"(val | 0x5));
}

static inline u64 read_cycles(void)
{
    u64 val;

    isb();  /* Instruction barrier for accurate timing */
    asm volatile("mrs %0, pmccntr_el0" : "=r"(val));
    return val;
}

/*
 * Timer callback - captures handler timestamp
 */
static enum hrtimer_restart timer_handler(struct hrtimer *t)
{
    unsigned long flags;

    /* Capture timestamp immediately */
    handler_cycles = read_cycles();
    handler_ns = ktime_get_ns();

    spin_lock_irqsave(&lock, flags);
    if (meas_pending) {
        meas_pending = 0;
        meas_done = 1;
    }
    spin_unlock_irqrestore(&lock, flags);

    return HRTIMER_NORESTART;
}

/*
 * Perform latency measurement
 */
static int measure_latency(struct int_latency_result *res)
{
    unsigned long flags;
    int timeout = 1000;  /* 1ms max wait */

    res->valid = 0;
    res->latency_cycles = 0;
    res->latency_ns = 0;

    spin_lock_irqsave(&lock, flags);
    meas_pending = 1;
    meas_done = 0;

    /* Capture trigger timestamp and start timer */
    trigger_ns = ktime_get_ns();
    trigger_cycles = read_cycles();

    /* Timer fires in 1ns (minimum) - measures interrupt delivery overhead */
    hrtimer_start(&timer, ktime_set(0, 1), HRTIMER_MODE_REL);
    spin_unlock_irqrestore(&lock, flags);

    /* Wait for handler to complete */
    while (!meas_done && timeout-- > 0)
        udelay(1);

    if (meas_done) {
        res->latency_cycles = handler_cycles - trigger_cycles;
        res->latency_ns = handler_ns - trigger_ns;
        res->valid = 1;
        return 0;
    }

    /* Timeout - cancel timer and report failure */
    meas_pending = 0;
    hrtimer_cancel(&timer);
    return -ETIMEDOUT;
}

/*
 * IOCTL handler
 */
static long dev_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct int_latency_result res;

    if (cmd != INT_LATENCY_MEASURE)
        return -ENOTTY;

    measure_latency(&res);

    if (copy_to_user((void __user *)arg, &res, sizeof(res)))
        return -EFAULT;

    return 0;
}

static int dev_open(struct inode *i, struct file *f)
{
    enable_cycle_counter();
    return 0;
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = dev_open,
    .unlocked_ioctl = dev_ioctl,
    .compat_ioctl   = dev_ioctl,
};

static struct miscdevice mdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = INT_LATENCY_DEVICE_NAME,
    .fops  = &fops,
    .mode  = 0666,
};

/*
 * Module init
 */
static int __init mod_init(void)
{
    int ret;

    spin_lock_init(&lock);

    /* Initialize high-resolution timer */
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer.function = timer_handler;

    ret = misc_register(&mdev);
    if (ret) {
        pr_err(DRIVER_NAME ": failed to register device: %d\n", ret);
        return ret;
    }

    enable_cycle_counter();

    pr_info(DRIVER_NAME " v" DRIVER_VERSION " loaded\n");
    pr_info(DRIVER_NAME ": device at /dev/%s\n", INT_LATENCY_DEVICE_NAME);

    return 0;
}

/*
 * Module exit
 */
static void __exit mod_exit(void)
{
    hrtimer_cancel(&timer);
    misc_deregister(&mdev);
    pr_info(DRIVER_NAME " unloaded\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Interrupt Latency Project");
MODULE_DESCRIPTION("Software interrupt latency measurement using hrtimer");
MODULE_VERSION(DRIVER_VERSION);
