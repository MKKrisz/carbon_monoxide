BINARY = co_test
OBJECTS = test.o co_impl_common.o co_impl_amd64_linux.o
HEADERS = co.h

CC = gcc
CFLAGS = -std=gnu23 -Wall -Wdeprecated -pedantic -g -O3
LDFLAGS = -g

.PHONY: all clean

all: $(BINARY)

clean:
	rm -f $(BINARY) $(OBJECTS)

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
