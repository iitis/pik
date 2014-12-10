CC ?= gcc
CFLAGS ?= -Wall

TARGETS = client server

default: all
all: $(TARGETS)

client: client.c measure.c
	$(CC) $(CFLAGS) measure.c client.c -o client

server: server.c
	$(CC) -static $(CFLAGS) server.c -o server -lpjf -lpcre

.PHONY: clean
clean:
	rm -f $(TARGETS)
