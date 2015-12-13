CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c11 -O2

.PHONY: all clean

all: wavhdr

wavhdr: wavhdr.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -v wavhdr
