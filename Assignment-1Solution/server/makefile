#Makefile#

CC=gcc
all: server

client: uftp_server.c
	$(CC) uftp_server.c -o server

clean:
	rm -f server