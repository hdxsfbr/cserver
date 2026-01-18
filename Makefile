CC=gcc
CFLAGS=-I include

all: main

main: src/main.c src/contacts.c
	$(CC) -o main src/main.c src/contacts.c $(CFLAGS)

clean:
	rm -f main
