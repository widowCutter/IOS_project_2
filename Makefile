binary=proj2

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread

proj2: proj2.c proj2.h
	$(CC) $(CFLAGS) $< -o $(binary)

run: proj2
	./proj2
