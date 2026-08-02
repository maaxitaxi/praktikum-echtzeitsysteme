CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpthread

all: server client peer

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

peer: peer.c
	$(CC) $(CFLAGS) -o peer peer.c $(LDFLAGS)

clean:
	rm -f server client peer

.PHONY: all clean