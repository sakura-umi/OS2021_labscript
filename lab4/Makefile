CFLAGS=$(shell pkg-config fuse --cflags) -g
LIBS=$(shell pkg-config fuse --libs)

CC=gcc
all: simple_fat16

simple_fat16: simple_fat16.o
	$(CC) -o $@ $^ $(LIBS)

simple_fat16.o: simple_fat16.c bench.h

clean:
	rm -f simple_fat16 *.o
