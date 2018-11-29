CC = gcc
CFLAGS = -c -std=c99

all: server client test

server: server.o thread_pool.o shared.o
	$(CC) -o $@ $^ -pthread

client: client.o shared.o
	$(CC) -o $@ $^

server.o: server.c
	$(CC) $(CFLAGS) -o $@ $<

client.o: client.c
	$(CC) $(CFLAGS) -o $@ $<

thread_pool.o: thread_pool.c
	$(CC) $(CFLAGS) -o $@ $<

shared.o: shared.c
	$(CC) $(CFLAGS) -o $@ $<

