// SPDX-License-Identifier: GPL-2.0
/*
 * pagewalk_driver.c - Kernel driver for precise pagewalk timing measurement
 *
 * Simple driver that:
 * 1. Takes a PID and virtual address from userspace
 * 2. Walks the page table to check validity
 * 3. Returns physical address and precise cycle count measurement
 *
 * Uses ARM64 cycle counter (PMCCNTR_EL0) for precise timing
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

#include "../include/pagewalk_ioctl.h"

#define DRIVER_NAME     "pagewalk_timer"
#define DRIVER_VERSION  "2.0"

/*
 * ARM64 cycle counter access
 * Enable and read the CPU cycle counter for precise timing
 */
#ifdef CONFIG_ARM64

static inline void enable_cycle_counter(void)
{
    u64 val;
    
    /* Enable user-space access to cycle counter (for completeness) */
    asm volatile("mrs %0, pmuserenr_el0" : "=r" (val));
    val |= (1 << 0) | (1 << 2);  /* EN | CR */
    asm volatile("msr pmuserenr_el0, %0" : : "r" (val));
    
    /* Enable cycle counter */
    asm volatile("mrs %0, pmcntenset_el0" : "=r" (val));
    val |= (1UL << 31);  /* Enable PMCCNTR_EL0 */
    asm volatile("msr pmcntenset_el0, %0" : : "r" (val));
    
    /* Configure PMCR: enable, reset cycle counter */
    asm volatile("mrs %0, pmcr_el0" : "=r" (val));
    val |= (1 << 0) | (1 << 2);  /* E | C */
    asm volatile("msr pmcr_el0, %0" : : "r" (val));
}

static inline u64 read_cycle_counter(void)
{
    u64 val;
    isb();  /* Instruction barrier for accurate timing */
    asm volatile("mrs %0, pmccntr_el0" : "=r" (val));
    return val;
}

#else
/* Fallback for non-ARM64 - use ktime */
static inline void enable_cycle_counter(void) {}
static inline u64 read_cycle_counter(void) { return ktime_get_ns(); }
#endif

/*
 * Perform page table walk with precise cycle timing
 */
static int do_pagewalk(struct mm_struct *mm, unsigned long vaddr,
                       struct pagewalk_request *req)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long pfn;
    u64 start_cycles, end_cycles;
    u64 start_ns, end_ns;
    int result = ADDR_VALID;

    req->paddr = 0;
    req->is_huge_page = 0;
    
    /* Get both cycle count and nanoseconds for comparison */
    start_ns = ktime_get_ns();
    start_cycles = read_cycle_counter();

    /* ============ PAGE TABLE WALK START ============ */
    
    /* Level 0: PGD (Page Global Directory) */
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        result = ADDR_INVALID_PGD;
        goto out;
    }

    /* Level 1: P4D (only on 5-level paging, pass-through on 4-level) */
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        result = ADDR_INVALID_P4D;
        goto out;
    }

    /* Level 2: PUD (Page Upper Directory) */
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        result = ADDR_INVALID_PUD;
        goto out;
    }

    /* Level 3: PMD (Page Middle Directory) */
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) {
        result = ADDR_INVALID_PMD;
        goto out;
    }

    /* Check for huge page (2MB section on ARM64) */
#ifdef CONFIG_ARM64
    if (pmd_sect(*pmd)) {
#else
    if (pmd_large(*pmd)) {
#endif
        if (!pmd_present(*pmd)) {
            result = ADDR_NOT_PRESENT;
            goto out;
        }
        pfn = pmd_pfn(*pmd);
        req->paddr = (pfn << PAGE_SHIFT) | (vaddr & ~PMD_MASK);
        req->is_huge_page = 1;
        goto out;
    }

    /* Level 4: PTE (Page Table Entry) - 4KB pages */
    pte = pte_offset_kernel(pmd, vaddr);
    if (!pte || pte_none(*pte)) {
        result = ADDR_INVALID_PTE;
        goto out;
    }

    if (!pte_present(*pte)) {
        result = ADDR_NOT_PRESENT;
        goto out;
    }

    /* Extract physical address */
    pfn = pte_pfn(*pte);
    req->paddr = (pfn << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);

    /* ============ PAGE TABLE WALK END ============ */

out:
    end_cycles = read_cycle_counter();
    end_ns = ktime_get_ns();
    
    req->walk_cycles = end_cycles - start_cycles;
    req->walk_ns = end_ns - start_ns;
    req->valid = result;
    
    return 0;
}

/*
 * Main pagewalk function - looks up process and performs walk
 */
static int pagewalk_for_pid(struct pagewalk_request *req)
{
    struct task_struct *task;
    struct mm_struct *mm;
    int ret;

    req->valid = ADDR_PROCESS_NOT_FOUND;
    req->paddr = 0;
    req->walk_cycles = 0;
    req->walk_ns = 0;

    /* Find the target process */
    rcu_read_lock();
    task = pid_task(find_vpid(req->pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return -ESRCH;
    }
    
    /* Get mm_struct with reference */
    mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm) {
        req->valid = ADDR_NO_MM;
        return -EINVAL;
    }

    /* Lock mm and perform pagewalk */
    mmap_read_lock(mm);
    ret = do_pagewalk(mm, req->vaddr, req);
    mmap_read_unlock(mm);
    
    mmput(mm);
    return ret;
}

/*
 * IOCTL handler
 */
static long pagewalk_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct pagewalk_request req;
    int ret;

    if (cmd != PAGEWALK_CHECK)
        return -ENOTTY;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    ret = pagewalk_for_pid(&req);
    
    /* Always copy back results (even on error, timing may be useful) */
    if (copy_to_user((void __user *)arg, &req, sizeof(req)))
        return -EFAULT;

    return ret;
}

/*
 * File operations
 */
static int pagewalk_open(struct inode *inode, struct file *file)
{
    /* Enable cycle counter on open */
    enable_cycle_counter();
    return 0;
}

static int pagewalk_release(struct inode *inode, struct file *file)
{
    return 0;
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

/*
 * Module init/exit
 */
static int __init pagewalk_init(void)
{
    int ret;

    ret = misc_register(&pagewalk_misc);
    if (ret) {
        pr_err(DRIVER_NAME ": failed to register device: %d\n", ret);
        return ret;
    }

    /* Enable cycle counter at module load */
    enable_cycle_counter();

    pr_info(DRIVER_NAME " v" DRIVER_VERSION " loaded\n");
    pr_info(DRIVER_NAME ": device at /dev/%s\n", PAGEWALK_DEVICE_NAME);
    
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
MODULE_DESCRIPTION("Precise pagewalk timing measurement using CPU cycle counter");
MODULE_VERSION(DRIVER_VERSION);
