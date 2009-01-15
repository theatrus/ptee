CC=gcc
CFLAGS=-O2 -Wall -std=gnu99 
PREFIX=/usr/local
all: ptee

ptee: ptee.c
	$(CC) $(CFLAGS) -o ptee ptee.c

install: ptee
	install ptee  -D $(PREFIX)/bin
	install "ptee.1" -D $(PREFIX)/share/man/man1
