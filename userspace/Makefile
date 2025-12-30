# Makefile for userspace test applications

# Cross-compilation settings
CC ?= aarch64-poky-linux-gcc
SYSROOT ?= /opt/fsl-imx-fb/6.6-scarthgap/sysroots/armv8a-poky-linux

CFLAGS := -Wall -Wextra -O2 -g -I../include --sysroot=$(SYSROOT)
LDFLAGS := --sysroot=$(SYSROOT)

TARGETS := pagewalk_test int_latency_test
SOURCES := pagewalk_test.c int_latency_test.c

.PHONY: all clean

all: $(TARGETS)

pagewalk_test: pagewalk_test.o
	$(CC) $(LDFLAGS) -o $@ $^

int_latency_test: int_latency_test.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS) *.o

# Dependencies
pagewalk_test.o: pagewalk_test.c ../include/pagewalk_ioctl.h
int_latency_test.o: int_latency_test.c ../include/int_latency_ioctl.h
