CC ?= gcc
CFLAGS ?= -Wall

TARGETS = client server

default: all
all: $(TARGETS)

client: client.c
	$(CC) $(CFLAGS) measure.c client.c -o client -static

server: server.c
	$(CC) $(CFLAGS) server.c -o server -static

.PHONY: clean
clean:
	rm -f $(TARGETS)
