CC=gcc
CFLAGS= -Wall -Werror -g

all: xel

xel: shell.o
	$(CC) -o $@ $<

clean:
	rm -f xel
	rm -f *.o
