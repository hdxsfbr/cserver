CC=gcc
CFLAGS=-I include

all: main

main: src/main.c src/contacts.c
	$(CC) $(CFLAGS) -o main src/main.c src/contacts.c

clean:
	rm -f main
