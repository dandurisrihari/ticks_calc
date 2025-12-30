# Makefile for pagewalk_test userspace application

# Cross-compiler - override with CC=aarch64-poky-linux-gcc
CC ?= aarch64-poky-linux-gcc

# Sysroot for cross-compilation (set via environment or override)
# Example: SYSROOT=/opt/poky/sysroots/aarch64-poky-linux
SYSROOT ?=

# Compiler flags
ifdef SYSROOT
CFLAGS := -Wall -Wextra -O2 -g -I../include --sysroot=$(SYSROOT)
LDFLAGS := -static --sysroot=$(SYSROOT)
else
CFLAGS := -Wall -Wextra -O2 -g -I../include
LDFLAGS := -static
endif

TARGET := pagewalk_test
SOURCES := pagewalk_test.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)

# Dependencies
pagewalk_test.o: pagewalk_test.c ../include/pagewalk_ioctl.h
