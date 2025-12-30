// SPDX-License-Identifier: GPL-2.0
/*
 * pagewalk_driver.c - Kernel driver for measuring pagewalk timing and interrupt latency
 *
 * This driver provides:
 * 1. Synchronous pagewalk timing measurement
 * 2. Interrupt-triggered pagewalk timing (using hrtimer)
 * 3. Statistics collection for multiple samples
 *
 * Usage: Load module, then use ioctl via /dev/pagewalk_timer
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/pgtable.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/highmem.h>

#include "../include/pagewalk_ioctl.h"

#define DRIVER_NAME     "pagewalk_timer"
#define DRIVER_VERSION  "1.0"

/* Per-file private data */
struct pagewalk_context {
    pid_t target_pid;
    unsigned long target_vaddr;
    struct pagewalk_result last_result;
    struct interrupt_timing irq_timing;
    struct hrtimer timer;
    wait_queue_head_t wait_queue;
    spinlock_t lock;
    bool measurement_pending;
    bool measurement_complete;
};

/*
 * Perform manual page table walk
 * Returns: validity code (ADDR_VALID on success, ADDR_INVALID_* on failure)
 * Sets *paddr to physical address if valid
 */
static int do_pagewalk(struct mm_struct *mm, unsigned long vaddr, 
                       unsigned long *paddr, u64 *walk_time_ns)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long pfn;
    u64 start_ns, end_ns;
    int result = ADDR_VALID;

    *paddr = 0;
    
    start_ns = ktime_get_ns();

    /* Walk the page table hierarchy */
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        result = ADDR_INVALID_PGD;
        goto out;
    }

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        result = ADDR_INVALID_P4D;
        goto out;
    }

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        result = ADDR_INVALID_PUD;
        goto out;
    }

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) {
        result = ADDR_INVALID_PMD;
        goto out;
    }

    /* Check for huge page (2MB) */
    if (pmd_large(*pmd)) {
        if (!pmd_present(*pmd)) {
            result = ADDR_NOT_PRESENT;
            goto out;
        }
        pfn = pmd_pfn(*pmd);
        *paddr = (pfn << PAGE_SHIFT) | (vaddr & ~PMD_MASK);
        goto out;
    }

    pte = pte_offset_map(pmd, vaddr);
    if (!pte) {
        result = ADDR_INVALID_PTE;
        goto out;
    }

    if (pte_none(*pte)) {
        pte_unmap(pte);
        result = ADDR_INVALID_PTE;
        goto out;
    }

    if (!pte_present(*pte)) {
        pte_unmap(pte);
        result = ADDR_NOT_PRESENT;
        goto out;
    }

    /* Extract physical address */
    pfn = pte_pfn(*pte);
    *paddr = (pfn << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);
    pte_unmap(pte);

out:
    end_ns = ktime_get_ns();
    *walk_time_ns = end_ns - start_ns;
    return result;
}

/*
 * Perform pagewalk for a given PID and virtual address
 */
static int pagewalk_for_pid(pid_t pid, unsigned long vaddr,
                           struct pagewalk_result *result)
{
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long paddr = 0;
    u64 walk_time_ns = 0;
    int validity;

    result->vaddr = vaddr;
    result->paddr = 0;
    result->valid = ADDR_PROCESS_NOT_FOUND;
    result->pagewalk_time_ns = 0;

    /* Find the task by PID */
    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return -ESRCH;
    }
    
    /* Get mm_struct (increments refcount) */
    mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm) {
        result->valid = ADDR_NO_MM;
        return -EINVAL;
    }

    /* Lock mm for reading */
    mmap_read_lock(mm);
    
    /* Perform the pagewalk */
    validity = do_pagewalk(mm, vaddr, &paddr, &walk_time_ns);
    
    mmap_read_unlock(mm);
    mmput(mm);

    result->valid = validity;
    result->paddr = paddr;
    result->pagewalk_time_ns = walk_time_ns;

    return 0;
}

/*
 * hrtimer callback - simulates interrupt handler
 * This runs in hard IRQ context
 */
static enum hrtimer_restart pagewalk_timer_callback(struct hrtimer *timer)
{
    struct pagewalk_context *ctx = container_of(timer, struct pagewalk_context, timer);
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long paddr = 0;
    u64 walk_time_ns = 0;
    unsigned long flags;

    /* Record handler entry time */
    ctx->irq_timing.handler_entry_ns = ktime_get_ns();

    spin_lock_irqsave(&ctx->lock, flags);

    /* Find task and get mm */
    rcu_read_lock();
    task = pid_task(find_vpid(ctx->target_pid), PIDTYPE_PID);
    if (!task) {
        ctx->irq_timing.valid = ADDR_PROCESS_NOT_FOUND;
        rcu_read_unlock();
        goto out;
    }
    
    mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm) {
        ctx->irq_timing.valid = ADDR_NO_MM;
        goto out;
    }

    /* Note: In real interrupt context, we shouldn't hold mm locks
     * For this measurement, we use hrtimer which can be configured
     * to run in softirq context. The mm lock is safe in that context.
     */
    ctx->irq_timing.pagewalk_start_ns = ktime_get_ns();
    
    if (mmap_read_trylock(mm)) {
        ctx->irq_timing.valid = do_pagewalk(mm, ctx->target_vaddr, &paddr, &walk_time_ns);
        mmap_read_unlock(mm);
    } else {
        ctx->irq_timing.valid = ADDR_NO_MM; /* Could not acquire lock */
    }
    
    ctx->irq_timing.pagewalk_end_ns = ktime_get_ns();
    mmput(mm);

out:
    ctx->irq_timing.handler_exit_ns = ktime_get_ns();
    
    /* Calculate latencies */
    ctx->irq_timing.interrupt_latency_ns = 
        ctx->irq_timing.handler_entry_ns - ctx->irq_timing.trigger_time_ns;
    ctx->irq_timing.total_latency_ns = 
        ctx->irq_timing.handler_exit_ns - ctx->irq_timing.trigger_time_ns;

    ctx->measurement_complete = true;
    ctx->measurement_pending = false;
    
    spin_unlock_irqrestore(&ctx->lock, flags);
    
    wake_up_interruptible(&ctx->wait_queue);

    return HRTIMER_NORESTART;
}

/*
 * Trigger interrupt-based measurement
 */
static int trigger_interrupt_measurement(struct pagewalk_context *ctx)
{
    unsigned long flags;
    ktime_t delay;

    spin_lock_irqsave(&ctx->lock, flags);
    
    if (ctx->measurement_pending) {
        spin_unlock_irqrestore(&ctx->lock, flags);
        return -EBUSY;
    }

    ctx->measurement_pending = true;
    ctx->measurement_complete = false;
    
    /* Clear previous timing data */
    memset(&ctx->irq_timing, 0, sizeof(ctx->irq_timing));
    
    /* Record trigger time */
    ctx->irq_timing.trigger_time_ns = ktime_get_ns();
    
    spin_unlock_irqrestore(&ctx->lock, flags);

    /* Schedule timer to fire in 1 microsecond */
    delay = ktime_set(0, 1000); /* 1 microsecond */
    hrtimer_start(&ctx->timer, delay, HRTIMER_MODE_REL);

    return 0;
}

/*
 * Run multiple measurements and collect statistics
 */
static int run_statistics(struct pagewalk_context *ctx, struct pagewalk_stats *stats)
{
    struct pagewalk_result result;
    u64 total_pagewalk = 0;
    u64 total_irq = 0;
    u32 count = stats->sample_count;
    u32 i;
    int ret;

    if (count == 0 || count > 1000)
        count = 10; /* Default to 10 samples */

    stats->min_pagewalk_ns = ULLONG_MAX;
    stats->max_pagewalk_ns = 0;
    stats->min_irq_latency_ns = ULLONG_MAX;
    stats->max_irq_latency_ns = 0;

    for (i = 0; i < count; i++) {
        /* Synchronous pagewalk measurement */
        ret = pagewalk_for_pid(ctx->target_pid, ctx->target_vaddr, &result);
        if (ret)
            continue;

        total_pagewalk += result.pagewalk_time_ns;
        
        if (result.pagewalk_time_ns < stats->min_pagewalk_ns)
            stats->min_pagewalk_ns = result.pagewalk_time_ns;
        if (result.pagewalk_time_ns > stats->max_pagewalk_ns)
            stats->max_pagewalk_ns = result.pagewalk_time_ns;

        /* Interrupt-based measurement */
        ret = trigger_interrupt_measurement(ctx);
        if (ret)
            continue;

        /* Wait for measurement to complete */
        wait_event_interruptible_timeout(ctx->wait_queue, 
                                         ctx->measurement_complete, 
                                         msecs_to_jiffies(100));

        if (ctx->measurement_complete) {
            total_irq += ctx->irq_timing.interrupt_latency_ns;
            
            if (ctx->irq_timing.interrupt_latency_ns < stats->min_irq_latency_ns)
                stats->min_irq_latency_ns = ctx->irq_timing.interrupt_latency_ns;
            if (ctx->irq_timing.interrupt_latency_ns > stats->max_irq_latency_ns)
                stats->max_irq_latency_ns = ctx->irq_timing.interrupt_latency_ns;
        }
    }

    stats->sample_count = count;
    stats->avg_pagewalk_ns = count ? total_pagewalk / count : 0;
    stats->avg_irq_latency_ns = count ? total_irq / count : 0;

    return 0;
}

/*
 * File operations
 */
static int pagewalk_open(struct inode *inode, struct file *file)
{
    struct pagewalk_context *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    spin_lock_init(&ctx->lock);
    init_waitqueue_head(&ctx->wait_queue);
    
    hrtimer_init(&ctx->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    ctx->timer.function = pagewalk_timer_callback;

    file->private_data = ctx;
    
    pr_info(DRIVER_NAME ": device opened\n");
    return 0;
}

static int pagewalk_release(struct inode *inode, struct file *file)
{
    struct pagewalk_context *ctx = file->private_data;

    if (ctx) {
        hrtimer_cancel(&ctx->timer);
        kfree(ctx);
    }

    pr_info(DRIVER_NAME ": device closed\n");
    return 0;
}

static long pagewalk_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct pagewalk_context *ctx = file->private_data;
    void __user *argp = (void __user *)arg;
    int ret = 0;

    switch (cmd) {
    case PAGEWALK_SET_PID: {
        __s32 pid;
        if (copy_from_user(&pid, argp, sizeof(pid)))
            return -EFAULT;
        ctx->target_pid = pid;
        pr_debug(DRIVER_NAME ": set target PID to %d\n", pid);
        break;
    }

    case PAGEWALK_SET_ADDR: {
        __u64 addr;
        if (copy_from_user(&addr, argp, sizeof(addr)))
            return -EFAULT;
        ctx->target_vaddr = addr;
        pr_debug(DRIVER_NAME ": set target address to 0x%llx\n", addr);
        break;
    }

    case PAGEWALK_CHECK: {
        struct pagewalk_result result;
        
        ret = pagewalk_for_pid(ctx->target_pid, ctx->target_vaddr, &result);
        if (ret)
            return ret;
        
        ctx->last_result = result;
        
        if (copy_to_user(argp, &result, sizeof(result)))
            return -EFAULT;
        break;
    }

    case PAGEWALK_TRIGGER_IRQ:
        ret = trigger_interrupt_measurement(ctx);
        break;

    case PAGEWALK_GET_IRQ_TIMING: {
        unsigned long flags;
        struct interrupt_timing timing;

        /* Wait for measurement if pending */
        if (ctx->measurement_pending) {
            ret = wait_event_interruptible_timeout(ctx->wait_queue,
                                                   ctx->measurement_complete,
                                                   msecs_to_jiffies(1000));
            if (ret == 0)
                return -ETIMEDOUT;
            if (ret < 0)
                return ret;
        }

        spin_lock_irqsave(&ctx->lock, flags);
        timing = ctx->irq_timing;
        spin_unlock_irqrestore(&ctx->lock, flags);

        if (copy_to_user(argp, &timing, sizeof(timing)))
            return -EFAULT;
        break;
    }

    case PAGEWALK_RUN_STATS: {
        struct pagewalk_stats stats;
        
        if (copy_from_user(&stats, argp, sizeof(stats)))
            return -EFAULT;
        
        ret = run_statistics(ctx, &stats);
        if (ret)
            return ret;
        
        if (copy_to_user(argp, &stats, sizeof(stats)))
            return -EFAULT;
        break;
    }

    case PAGEWALK_CHECK_FULL: {
        struct pagewalk_request req;
        struct pagewalk_result result;

        if (copy_from_user(&req, argp, sizeof(req)))
            return -EFAULT;

        ctx->target_pid = req.pid;
        ctx->target_vaddr = req.vaddr;

        ret = pagewalk_for_pid(req.pid, req.vaddr, &result);
        if (ret)
            return ret;

        ctx->last_result = result;

        if (copy_to_user(argp, &result, sizeof(result)))
            return -EFAULT;
        break;
    }

    default:
        return -ENOTTY;
    }

    return ret;
}

static const struct file_operations pagewalk_fops = {
    .owner          = THIS_MODULE,
    .open           = pagewalk_open,
    .release        = pagewalk_release,
    .unlocked_ioctl = pagewalk_ioctl,
    .compat_ioctl   = pagewalk_ioctl,
};

static struct miscdevice pagewalk_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = PAGEWALK_DEVICE_NAME,
    .fops  = &pagewalk_fops,
    .mode  = 0666,
};

static int __init pagewalk_init(void)
{
    int ret;

    ret = misc_register(&pagewalk_misc);
    if (ret) {
        pr_err(DRIVER_NAME ": failed to register misc device: %d\n", ret);
        return ret;
    }

    pr_info(DRIVER_NAME " v" DRIVER_VERSION " loaded\n");
    pr_info(DRIVER_NAME ": device created at /dev/%s\n", PAGEWALK_DEVICE_NAME);
    
    return 0;
}

static void __exit pagewalk_exit(void)
{
    misc_deregister(&pagewalk_misc);
    pr_info(DRIVER_NAME " unloaded\n");
}

module_init(pagewalk_init);
module_exit(pagewalk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pagewalk Timer Project");
MODULE_DESCRIPTION("Kernel driver for measuring pagewalk timing and interrupt latency");
MODULE_VERSION(DRIVER_VERSION);
