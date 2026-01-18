CC=gcc
CFLAGS=-I include -DNO_SSL -DNO_CGI

all: main

main: src/main.c src/contacts.c src/civetweb.c
	$(CC) $(CFLAGS) -o main src/main.c src/contacts.c src/civetweb.c -lpthread

clean:
	rm -f main
