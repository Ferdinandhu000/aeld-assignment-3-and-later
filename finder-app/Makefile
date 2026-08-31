CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g

.PHONY: all default clean

default: writer
all: writer

writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o

writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f writer *.o