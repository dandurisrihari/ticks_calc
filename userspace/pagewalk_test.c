/*
 * pagewalk_test.c - Simple userspace test for pagewalk timing
 *
 * Usage: ./pagewalk_test [address_hex]
 *   If no address given, uses address of a local variable
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

/* Perform pagewalk and print results */
static int do_pagewalk_test(int fd, pid_t pid, uint64_t vaddr, const char *label)
{
    struct pagewalk_request req;
    int ret;

    memset(&req, 0, sizeof(req));
    req.pid = pid;
    req.vaddr = vaddr;

    ret = ioctl(fd, PAGEWALK_CHECK, &req);
    
    printf("\n=== %s ===\n", label);
    printf("  PID:              %d\n", pid);
    printf("  Virtual Address:  0x%016" PRIx64 "\n", req.vaddr);
    printf("  Physical Address: 0x%016" PRIx64 "\n", req.paddr);
    printf("  Status:           %s\n", validity_str(req.valid));
    printf("  Huge Page:        %s\n", req.is_huge_page ? "Yes (2MB)" : "No (4KB)");
    printf("  Walk Time:        %" PRIu64 " cycles\n", req.walk_cycles);
    printf("  Walk Time:        %" PRIu64 " ns\n", req.walk_ns);
    
    if (ret < 0 && errno != 0) {
        printf("  Error:            %s\n", strerror(errno));
    }

    return (req.valid == ADDR_VALID) ? 0 : -1;
}

int main(int argc, char *argv[])
{
    int fd;
    pid_t pid = getpid();
    volatile int test_var = 0x12345678;  /* Variable to test */
    uint64_t test_addr;
    void *mmap_page = NULL;
    size_t page_size = sysconf(_SC_PAGESIZE);

    /* Open device */
    fd = open("/dev/" PAGEWALK_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/" PAGEWALK_DEVICE_NAME);
        printf("Make sure the kernel module is loaded: sudo insmod pagewalk_driver.ko\n");
        return 1;
    }

    printf("Pagewalk Timing Test\n");
    printf("====================\n");
    printf("PID: %d\n", pid);
    printf("Page size: %zu bytes\n", page_size);

    /* Test 1: Stack variable */
    test_addr = (uint64_t)(uintptr_t)&test_var;
    do_pagewalk_test(fd, pid, test_addr, "Stack Variable");

    /* Test 2: User-provided address */
    if (argc > 1) {
        test_addr = strtoull(argv[1], NULL, 16);
        do_pagewalk_test(fd, pid, test_addr, "User Address");
    }

    /* Test 3: Mmap'd page - before access */
    mmap_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_page != MAP_FAILED) {
        test_addr = (uint64_t)(uintptr_t)mmap_page;
        do_pagewalk_test(fd, pid, test_addr, "Mmap Page (before access)");

        /* Touch the page */
        *(volatile int *)mmap_page = 0xDEADBEEF;
        
        do_pagewalk_test(fd, pid, test_addr, "Mmap Page (after access)");

        munmap(mmap_page, page_size);
        
        do_pagewalk_test(fd, pid, test_addr, "Mmap Page (after munmap)");
    }

    /* Test 4: Invalid address */
    do_pagewalk_test(fd, pid, 0xDEADBEEF00000000ULL, "Invalid Address");

    close(fd);
    return 0;
}
