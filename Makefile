CC=gcc
CFLAGS=-I.

lamport_logical_clock: lamport_logical_clock.o
	     $(CC) -o lamport_clock lamport_logical_clock.o -I. -lpthread

clean:
	rm -rf lamport_clock lamport_logical_clock.o
