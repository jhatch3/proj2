# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source files and matching output names
SRCS = part1.c part2.c part3.c part4.c iobound.c cpubound.c
BINS = part1 part2 part3 part4 iobound cpubound

# Default target: build all
all: $(BINS)

# Build rule for each executable
part1: part1.c
	$(CC) $(CFLAGS) -o $@ $<

part2: part2.c
	$(CC) $(CFLAGS) -o $@ $<

part3: part3.c
	$(CC) $(CFLAGS) -o $@ $<

part4: part4.c
	$(CC) $(CFLAGS) -o $@ $<

iobound: iobound.c
	$(CC) $(CFLAGS) -o $@ $<

cpubound: cpubound.c
	$(CC) $(CFLAGS) -o $@ $<

# Clean target
clean:
	rm -f $(BINS)# Compiler
