CC=gcc
CFLAGS=-O2 -Wall -std=c99

all: ptee

ptee: ptee.c
	$(CC) $(CFLAGS) -o ptee ptee.c