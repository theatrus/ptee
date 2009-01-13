CC=gcc
CFLAGS=-O2 -Wall -std=gnu99 -g3

all: ptee

ptee: ptee.c
	$(CC) $(CFLAGS) -o ptee ptee.c