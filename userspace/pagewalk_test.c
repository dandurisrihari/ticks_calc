/*
 * pagewalk_test.c - Userspace test for pagewalk timing measurement
 *
 * Usage: ./pagewalk_test [count]
 *   count - number of samples for averaging (default: 1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <inttypes.h>

#include "../include/pagewalk_ioctl.h"

/* Convert validity code to string */
static const char *validity_str(uint32_t valid)
{
    switch (valid) {
    case ADDR_VALID:             return "VALID";
    case ADDR_INVALID_PGD:       return "INVALID (PGD)";
    case ADDR_INVALID_P4D:       return "INVALID (P4D)";
    case ADDR_INVALID_PUD:       return "INVALID (PUD)";
    case ADDR_INVALID_PMD:       return "INVALID (PMD)";
    case ADDR_INVALID_PTE:       return "INVALID (PTE)";
    case ADDR_NOT_PRESENT:       return "NOT PRESENT";
    case ADDR_PROCESS_NOT_FOUND: return "PROCESS NOT FOUND";
    case ADDR_NO_MM:             return "NO MM STRUCT";
    default:                     return "UNKNOWN";
    }
}

/* Perform single pagewalk */
static int do_pagewalk(int fd, pid_t pid, uint64_t vaddr, struct pagewalk_request *req)
{
    memset(req, 0, sizeof(*req));
    req->pid = pid;
    req->vaddr = vaddr;
    return ioctl(fd, PAGEWALK_CHECK, req);
}

/* Perform pagewalk with averaging */
static void do_pagewalk_avg(int fd, pid_t pid, uint64_t vaddr, const char *label, int count)
{
    struct pagewalk_request req;
    uint64_t total_cycles = 0, total_ns = 0;
    uint64_t min_cycles = UINT64_MAX, max_cycles = 0;
    uint64_t min_ns = UINT64_MAX, max_ns = 0;
    int valid_count = 0;
    int i;

    /* First call to get status info */
    do_pagewalk(fd, pid, vaddr, &req);

    printf("\n=== %s ===\n", label);
    printf("  Virtual Address:  0x%016" PRIx64 "\n", req.vaddr);
    printf("  Physical Address: 0x%016" PRIx64 "\n", req.paddr);
    printf("  Status:           %s\n", validity_str(req.valid));
    printf("  Huge Page:        %s\n", req.is_huge_page ? "Yes (2MB)" : "No (4KB)");

    if (count <= 1) {
        printf("  Walk Time:        %" PRIu64 " cycles, %" PRIu64 " ns\n",
               req.walk_cycles, req.walk_ns);
        return;
    }

    /* Collect multiple samples */
    for (i = 0; i < count; i++) {
        if (do_pagewalk(fd, pid, vaddr, &req) == 0 || req.walk_cycles > 0) {
            total_cycles += req.walk_cycles;
            total_ns += req.walk_ns;
            valid_count++;

            if (req.walk_cycles < min_cycles) min_cycles = req.walk_cycles;
            if (req.walk_cycles > max_cycles) max_cycles = req.walk_cycles;
            if (req.walk_ns < min_ns) min_ns = req.walk_ns;
            if (req.walk_ns > max_ns) max_ns = req.walk_ns;
        }
    }

    if (valid_count > 0) {
        printf("  Samples:          %d\n", valid_count);
        printf("  Avg Walk Time:    %" PRIu64 " cycles, %" PRIu64 " ns\n",
               total_cycles / valid_count, total_ns / valid_count);
        printf("  Min Walk Time:    %" PRIu64 " cycles, %" PRIu64 " ns\n",
               min_cycles, min_ns);
        printf("  Max Walk Time:    %" PRIu64 " cycles, %" PRIu64 " ns\n",
               max_cycles, max_ns);
    }
}

int main(int argc, char *argv[])
{
    int fd;
    pid_t pid = getpid();
    volatile int test_var = 0x12345678;  /* Variable to test */
    uint64_t test_addr;
    void *mmap_page = NULL;
    size_t page_size = sysconf(_SC_PAGESIZE);
    int count = 1;

    if (argc > 1)
        count = atoi(argv[1]);
    if (count < 1)
        count = 1;

    /* Open device */
    fd = open("/dev/" PAGEWALK_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/" PAGEWALK_DEVICE_NAME);
        printf("Load module: insmod pagewalk_driver.ko\n");
        return 1;
    }

    printf("Pagewalk Timing Test\n");
    printf("====================\n");
    printf("PID: %d, Page size: %zu bytes", pid, page_size);
    if (count > 1)
        printf(", Samples: %d", count);
    printf("\n");

    /* Test 1: Stack variable (always mapped) */
    test_addr = (uint64_t)(uintptr_t)&test_var;
    do_pagewalk_avg(fd, pid, test_addr, "Stack Variable", count);

    /* Test 2: Mmap'd page - before and after access */
    mmap_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_page != MAP_FAILED) {
        test_addr = (uint64_t)(uintptr_t)mmap_page;
        do_pagewalk_avg(fd, pid, test_addr, "Mmap Page (before access)", 1);

        /* Touch the page to fault it in */
        *(volatile int *)mmap_page = 0xDEADBEEF;

        do_pagewalk_avg(fd, pid, test_addr, "Mmap Page (after access)", count);

        munmap(mmap_page, page_size);

        do_pagewalk_avg(fd, pid, test_addr, "Mmap Page (after munmap)", 1);
    }

    /* Test 3: Invalid address */
    do_pagewalk_avg(fd, pid, 0xDEADBEEF00000000ULL, "Invalid Address", 1);

    close(fd);
    return 0;
}
