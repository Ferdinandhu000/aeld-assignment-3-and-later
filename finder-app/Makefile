# GNU make supplies a built-in default of `cc`, which would otherwise prevent
# `CC ?=` from honoring CROSS_COMPILE.  Respect an explicitly supplied CC while
# selecting the cross compiler for cross builds.
ifeq ($(origin CC),default)
CC = $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,cc)
endif
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
