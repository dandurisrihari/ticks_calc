/*
 * int_latency_test.c - Measure software interrupt latency
 *
 * Usage: ./int_latency_test [count]
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <inttypes.h>

#include "../include/int_latency_ioctl.h"

int main(int argc, char *argv[])
{
    int fd, i, count = 10;
    int valid_count = 0;
    struct int_latency_result res;
    uint64_t total_cycles = 0, total_ns = 0;
    uint64_t min_ns = UINT64_MAX, max_ns = 0;

    if (argc > 1)
        count = atoi(argv[1]);

    fd = open("/dev/" INT_LATENCY_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("open /dev/" INT_LATENCY_DEVICE_NAME);
        printf("Load module: insmod int_latency_driver.ko\n");
        return 1;
    }

    printf("Interrupt Latency Test (%d samples)\n", count);
    printf("====================================\n");
    printf("%5s  %12s  %12s\n", "#", "Cycles", "Nanoseconds");

    for (i = 0; i < count; i++) {
        if (ioctl(fd, INT_LATENCY_MEASURE, &res) < 0) {
            perror("ioctl");
            continue;
        }

        if (res.valid) {
            printf("%5d  %12" PRIu64 "  %12" PRIu64 " ns\n",
                   i + 1, res.latency_cycles, res.latency_ns);
            total_cycles += res.latency_cycles;
            total_ns += res.latency_ns;
            valid_count++;
            if (res.latency_ns < min_ns)
                min_ns = res.latency_ns;
            if (res.latency_ns > max_ns)
                max_ns = res.latency_ns;
        } else {
            printf("%5d  TIMEOUT\n", i + 1);
        }
        usleep(1000);  /* 1ms between samples */
    }

    if (valid_count > 0) {
        printf("------------------------------------\n");
        printf("Avg:   %12" PRIu64 "  %12" PRIu64 " ns\n",
               total_cycles / valid_count, total_ns / valid_count);
        printf("Min:                   %12" PRIu64 " ns\n", min_ns);
        printf("Max:                   %12" PRIu64 " ns\n", max_ns);
    }

    close(fd);
    return 0;
}
