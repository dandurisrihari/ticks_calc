/*
 * pagewalk_test.c - Userspace test application for pagewalk timing driver
 *
 * This application:
 * 1. Tests synchronous pagewalk timing
 * 2. Tests interrupt-triggered pagewalk timing
 * 3. Collects and displays statistics
 * 4. Can test against itself or a specified PID
 *
 * Usage: ./pagewalk_test [options]
 *   -p PID     : Target process PID (default: self)
 *   -a ADDR    : Virtual address to test (hex, default: address of test variable)
 *   -n COUNT   : Number of samples for statistics (default: 100)
 *   -s         : Run statistics mode
 *   -i         : Run interrupt latency test
 *   -h         : Show help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <stdint.h>
#include <inttypes.h>

#include "../include/pagewalk_ioctl.h"

/* Test variable whose address we'll use for pagewalk */
static volatile int test_variable = 0x12345678;

/* Convert validity code to string */
static const char *validity_str(uint32_t valid)
{
    switch (valid) {
    case ADDR_VALID:            return "VALID";
    case ADDR_INVALID_PGD:      return "INVALID (PGD)";
    case ADDR_INVALID_P4D:      return "INVALID (P4D)";
    case ADDR_INVALID_PUD:      return "INVALID (PUD)";
    case ADDR_INVALID_PMD:      return "INVALID (PMD)";
    case ADDR_INVALID_PTE:      return "INVALID (PTE)";
    case ADDR_NOT_PRESENT:      return "NOT PRESENT";
    case ADDR_PROCESS_NOT_FOUND: return "PROCESS NOT FOUND";
    case ADDR_NO_MM:            return "NO MM STRUCT";
    default:                    return "UNKNOWN";
    }
}

/* Print timing in both nanoseconds and microseconds */
static void print_timing(const char *label, uint64_t ns)
{
    printf("  %-30s: %8" PRIu64 " ns (%6.3f µs)\n", 
           label, ns, (double)ns / 1000.0);
}

/* Run a single synchronous pagewalk test */
static int test_sync_pagewalk(int fd, pid_t pid, uint64_t addr)
{
    struct pagewalk_result result;
    int ret;

    /* Set PID */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        return -1;
    }

    /* Set address */
    ret = ioctl(fd, PAGEWALK_SET_ADDR, &addr);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_ADDR");
        return -1;
    }

    /* Perform pagewalk */
    ret = ioctl(fd, PAGEWALK_CHECK, &result);
    if (ret < 0) {
        perror("ioctl PAGEWALK_CHECK");
        return -1;
    }

    printf("\n=== Synchronous Pagewalk Result ===\n");
    printf("  Target PID:          %d\n", pid);
    printf("  Virtual Address:     0x%016" PRIx64 "\n", result.vaddr);
    printf("  Physical Address:    0x%016" PRIx64 "\n", result.paddr);
    printf("  Validity:            %s\n", validity_str(result.valid));
    print_timing("Pagewalk Time", result.pagewalk_time_ns);

    return 0;
}

/* Run interrupt latency test */
static int test_interrupt_latency(int fd, pid_t pid, uint64_t addr)
{
    struct interrupt_timing timing;
    int ret;

    /* Set PID and address first */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        return -1;
    }

    ret = ioctl(fd, PAGEWALK_SET_ADDR, &addr);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_ADDR");
        return -1;
    }

    /* Trigger interrupt-based measurement */
    ret = ioctl(fd, PAGEWALK_TRIGGER_IRQ);
    if (ret < 0) {
        perror("ioctl PAGEWALK_TRIGGER_IRQ");
        return -1;
    }

    /* Get results */
    ret = ioctl(fd, PAGEWALK_GET_IRQ_TIMING, &timing);
    if (ret < 0) {
        perror("ioctl PAGEWALK_GET_IRQ_TIMING");
        return -1;
    }

    printf("\n=== Interrupt Latency Measurement ===\n");
    printf("  Target PID:          %d\n", pid);
    printf("  Virtual Address:     0x%016" PRIx64 "\n", addr);
    printf("  Validity:            %s\n", validity_str(timing.valid));
    printf("\n  Timing Breakdown:\n");
    print_timing("Trigger Time (ref)", 0);
    print_timing("Interrupt Latency", timing.interrupt_latency_ns);
    print_timing("Pagewalk Duration", 
                 timing.pagewalk_end_ns - timing.pagewalk_start_ns);
    print_timing("Total Handler Time", 
                 timing.handler_exit_ns - timing.handler_entry_ns);
    print_timing("Total Latency", timing.total_latency_ns);

    return 0;
}

/* Run statistics collection */
static int test_statistics(int fd, pid_t pid, uint64_t addr, uint32_t count)
{
    struct pagewalk_stats stats;
    int ret;

    /* Set PID and address first */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        return -1;
    }

    ret = ioctl(fd, PAGEWALK_SET_ADDR, &addr);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_ADDR");
        return -1;
    }

    /* Run statistics */
    memset(&stats, 0, sizeof(stats));
    stats.sample_count = count;

    printf("\nCollecting %u samples...\n", count);

    ret = ioctl(fd, PAGEWALK_RUN_STATS, &stats);
    if (ret < 0) {
        perror("ioctl PAGEWALK_RUN_STATS");
        return -1;
    }

    printf("\n=== Statistics (%u samples) ===\n", stats.sample_count);
    printf("  Target PID:          %d\n", pid);
    printf("  Virtual Address:     0x%016" PRIx64 "\n", addr);
    
    printf("\n  Pagewalk Timing:\n");
    print_timing("Min", stats.min_pagewalk_ns);
    print_timing("Max", stats.max_pagewalk_ns);
    print_timing("Average", stats.avg_pagewalk_ns);
    
    printf("\n  Interrupt Latency:\n");
    print_timing("Min", stats.min_irq_latency_ns);
    print_timing("Max", stats.max_irq_latency_ns);
    print_timing("Average", stats.avg_irq_latency_ns);

    return 0;
}

/* Test with an invalid address */
static int test_invalid_address(int fd, pid_t pid)
{
    struct pagewalk_result result;
    uint64_t invalid_addr = 0xDEADBEEFDEADBEEF;
    int ret;

    /* Set PID and invalid address */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        return -1;
    }
    
    ret = ioctl(fd, PAGEWALK_SET_ADDR, &invalid_addr);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_ADDR");
        return -1;
    }
    
    ret = ioctl(fd, PAGEWALK_CHECK, &result);
    /* Note: ioctl may succeed but return invalid status in result */

    printf("\n=== Invalid Address Test ===\n");
    printf("  Target PID:          %d\n", pid);
    printf("  Virtual Address:     0x%016" PRIx64 " (intentionally invalid)\n", invalid_addr);
    printf("  Validity:            %s\n", validity_str(result.valid));
    print_timing("Pagewalk Time", result.pagewalk_time_ns);

    return 0;
}

/*
 * Test with mmap'd pages - demonstrates kernel validation of mapped pages
 * This test:
 * 1. Maps a page using mmap()
 * 2. Verifies kernel sees it as VALID
 * 3. Accesses the page (ensures it's faulted in)
 * 4. Verifies again after access
 * 5. Unmaps the page
 * 6. Verifies kernel sees it as INVALID after unmap
 */
static int test_mmap_page_validity(int fd, pid_t pid)
{
    struct pagewalk_result result;
    void *mapped_page;
    size_t page_size = sysconf(_SC_PAGESIZE);
    uint64_t vaddr;
    int ret;

    printf("\n=== MMAP Page Validity Test ===\n");
    printf("  Page size: %zu bytes\n", page_size);
    
    /* Step 1: Allocate a page using mmap */
    mapped_page = mmap(NULL, page_size, 
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
    
    if (mapped_page == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    
    vaddr = (uint64_t)(uintptr_t)mapped_page;
    printf("  Mapped page at:      0x%016" PRIx64 "\n", vaddr);
    
    /* Step 2: Check validity BEFORE accessing the page */
    /* Note: Page may not be physically allocated yet (demand paging) */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        munmap(mapped_page, page_size);
        return -1;
    }
    
    ret = ioctl(fd, PAGEWALK_SET_ADDR, &vaddr);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_ADDR");
        munmap(mapped_page, page_size);
        return -1;
    }
    
    ret = ioctl(fd, PAGEWALK_CHECK, &result);
    if (ret < 0) {
        perror("ioctl PAGEWALK_CHECK");
        munmap(mapped_page, page_size);
        return -1;
    }
    
    printf("\n  [Before access - page may not be faulted in]\n");
    printf("  Virtual Address:     0x%016" PRIx64 "\n", result.vaddr);
    printf("  Physical Address:    0x%016" PRIx64 "\n", result.paddr);
    printf("  Is Mapped/Valid:     %s\n", 
           result.valid == ADDR_VALID ? "YES - PAGE IS MAPPED" : validity_str(result.valid));
    print_timing("Pagewalk Time", result.pagewalk_time_ns);
    
    /* Step 3: Access the page to ensure it's faulted in */
    printf("\n  >> Writing to page to trigger page fault...\n");
    memset(mapped_page, 0xAB, page_size);  /* Write to entire page */
    volatile int *ptr = (volatile int *)mapped_page;
    *ptr = 0x12345678;  /* Ensure write is not optimized out */
    (void)*ptr;         /* Ensure read happens */
    
    /* Step 4: Check validity AFTER accessing the page */
    ret = ioctl(fd, PAGEWALK_CHECK, &result);
    if (ret < 0) {
        perror("ioctl PAGEWALK_CHECK");
        munmap(mapped_page, page_size);
        return -1;
    }
    
    printf("\n  [After access - page should be faulted in]\n");
    printf("  Virtual Address:     0x%016" PRIx64 "\n", result.vaddr);
    printf("  Physical Address:    0x%016" PRIx64 "\n", result.paddr);
    printf("  Is Mapped/Valid:     %s\n", 
           result.valid == ADDR_VALID ? "YES - PAGE IS MAPPED" : validity_str(result.valid));
    print_timing("Pagewalk Time", result.pagewalk_time_ns);
    
    /* Step 5: Unmap the page */
    printf("\n  >> Unmapping page with munmap()...\n");
    if (munmap(mapped_page, page_size) != 0) {
        perror("munmap failed");
        return -1;
    }
    
    /* Step 6: Check validity AFTER unmapping - should be INVALID */
    ret = ioctl(fd, PAGEWALK_CHECK, &result);
    if (ret < 0) {
        /* Expected behavior - the address is now unmapped */
        printf("\n  [After munmap - page should be unmapped]\n");
        printf("  ioctl returned error (expected for unmapped address)\n");
    } else {
        printf("\n  [After munmap - page should be unmapped]\n");
        printf("  Virtual Address:     0x%016" PRIx64 "\n", result.vaddr);
        printf("  Physical Address:    0x%016" PRIx64 "\n", result.paddr);
        printf("  Is Mapped/Valid:     %s\n", 
               result.valid == ADDR_VALID ? "YES (unexpected!)" : 
               "NO - PAGE IS NOT MAPPED (expected)");
        print_timing("Pagewalk Time", result.pagewalk_time_ns);
    }
    
    printf("\n  Test complete: Demonstrated kernel pagewalk validity detection\n");
    
    return 0;
}

/*
 * Test multiple mapped pages at once
 */
static int test_multiple_mapped_pages(int fd, pid_t pid)
{
    struct pagewalk_result result;
    void *pages[4];
    size_t page_size = sysconf(_SC_PAGESIZE);
    int i;
    int ret;

    printf("\n=== Multiple Mapped Pages Test ===\n");
    
    /* Map 4 pages */
    for (i = 0; i < 4; i++) {
        pages[i] = mmap(NULL, page_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
        if (pages[i] == MAP_FAILED) {
            perror("mmap failed");
            while (--i >= 0)
                munmap(pages[i], page_size);
            return -1;
        }
        /* Touch the page to ensure it's allocated */
        ((volatile char *)pages[i])[0] = (char)i;
    }
    
    printf("  Mapped 4 pages:\n");
    
    /* Set PID once */
    ret = ioctl(fd, PAGEWALK_SET_PID, &pid);
    if (ret < 0) {
        perror("ioctl PAGEWALK_SET_PID");
        for (i = 0; i < 4; i++)
            munmap(pages[i], page_size);
        return -1;
    }
    
    /* Check each page */
    for (i = 0; i < 4; i++) {
        uint64_t vaddr = (uint64_t)(uintptr_t)pages[i];
        
        ret = ioctl(fd, PAGEWALK_SET_ADDR, &vaddr);
        if (ret < 0) {
            perror("ioctl PAGEWALK_SET_ADDR");
            continue;
        }
        
        ret = ioctl(fd, PAGEWALK_CHECK, &result);
        if (ret < 0) {
            perror("ioctl PAGEWALK_CHECK");
            continue;
        }
        
        printf("  Page %d: vaddr=0x%016" PRIx64 " paddr=0x%016" PRIx64 " valid=%s time=%" PRIu64 "ns (%.3fµs)\n",
               i, result.vaddr, result.paddr,
               result.valid == ADDR_VALID ? "YES" : "NO",
               result.pagewalk_time_ns, (double)result.pagewalk_time_ns / 1000.0);
    }
    
    /* Unmap all pages */
    for (i = 0; i < 4; i++) {
        munmap(pages[i], page_size);
    }
    
    printf("  Pages unmapped.\n");
    
    return 0;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("\nOptions:\n");
    printf("  -p PID     Target process PID (default: self)\n");
    printf("  -a ADDR    Virtual address to test in hex (default: test variable)\n");
    printf("  -n COUNT   Number of samples for statistics (default: 100)\n");
    printf("  -s         Run statistics mode\n");
    printf("  -i         Run interrupt latency test\n");
    printf("  -v         Run invalid address test\n");
    printf("  -m         Run mmap page validity test (map/access/unmap)\n");
    printf("  -M         Run multiple mapped pages test\n");
    printf("  -A         Run all tests\n");
    printf("  -h         Show this help\n");
    printf("\nExamples:\n");
    printf("  %s                    # Test self with default address\n", prog);
    printf("  %s -p 1234 -a 0x7fff12345000\n", prog);
    printf("  %s -s -n 1000         # Collect 1000 samples\n", prog);
    printf("  %s -m                 # Test mmap page validity\n", prog);
    printf("  %s -A                 # Run all tests\n", prog);
}

int main(int argc, char *argv[])
{
    int fd;
    int opt;
    pid_t pid = getpid();
    uint64_t addr = (uint64_t)(uintptr_t)&test_variable;
    uint32_t sample_count = 100;
    int run_stats = 0;
    int run_irq = 0;
    int run_invalid = 0;
    int run_mmap = 0;
    int run_multi_mmap = 0;
    int run_all = 0;

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "p:a:n:sivmMAh")) != -1) {
        switch (opt) {
        case 'p':
            pid = atoi(optarg);
            break;
        case 'a':
            addr = strtoull(optarg, NULL, 16);
            break;
        case 'n':
            sample_count = atoi(optarg);
            break;
        case 's':
            run_stats = 1;
            break;
        case 'i':
            run_irq = 1;
            break;
        case 'v':
            run_invalid = 1;
            break;
        case 'm':
            run_mmap = 1;
            break;
        case 'M':
            run_multi_mmap = 1;
            break;
        case 'A':
            run_all = 1;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    /* Open the device */
    fd = open(PAGEWALK_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open " PAGEWALK_DEVICE_PATH);
        printf("\nMake sure the kernel module is loaded:\n");
        printf("  sudo insmod kernel/pagewalk_driver.ko\n");
        return 1;
    }

    printf("Pagewalk Timing Test Application\n");
    printf("================================\n");
    printf("Device: %s\n", PAGEWALK_DEVICE_PATH);
    printf("Self PID: %d\n", getpid());
    printf("Test variable address: 0x%016" PRIx64 "\n", 
           (uint64_t)(uintptr_t)&test_variable);

    /* Run tests */
    if (run_all || (!run_stats && !run_irq && !run_invalid && !run_mmap && !run_multi_mmap)) {
        /* Default: run sync pagewalk */
        test_sync_pagewalk(fd, pid, addr);
    }

    if (run_all || run_irq) {
        test_interrupt_latency(fd, pid, addr);
    }

    if (run_all || run_stats) {
        test_statistics(fd, pid, addr, sample_count);
    }

    if (run_all || run_invalid) {
        test_invalid_address(fd, pid);
    }

    if (run_all || run_mmap) {
        test_mmap_page_validity(fd, pid);
    }

    if (run_all || run_multi_mmap) {
        test_multiple_mapped_pages(fd, pid);
    }

    printf("\n");
    close(fd);
    return 0;
}
