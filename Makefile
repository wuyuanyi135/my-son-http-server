CC=gcc
CFLAGS=-I. -c -DMG_ENABLE_HTTP_STREAMING_MULTIPART -g

LD=gcc
LDFLAGS = -L/usr/local/lib
execute: server
	./server

mongoose.o: mongoose.c
	$(CC) $(CFLAGS) -o mongoose.o mongoose.c
main.o: main.c
	$(CC) $(CFLAGS) -o main.o main.c
server: main.o mongoose.o
	$(LD) $(LDFLAGS) -o server main.o mongoose.o


