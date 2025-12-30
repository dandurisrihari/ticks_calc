# Makefile for pagewalk_test userspace application

CC := gcc
CFLAGS := -Wall -Wextra -O2 -g -I../include
LDFLAGS :=

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
