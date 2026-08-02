CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server client peer

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

peer: peer.c
	$(CC) $(CFLAGS) -o peer peer.c

clean:
	rm -f server client peer

.PHONY: all clean
