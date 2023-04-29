binary=proj2

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic

proj2: proj2.c proj2.h Makefile
	$(CC) $(CFLAGS) $< -o $(binary)

run: proj2
	./proj2 5 1 100 20 100
	
	# ./proj2 12 2 100 20 -1 | true # Wrong args
deadlock: proj2
	@./deadlock.sh 5 3 0 0 0
	# @./deadlock.sh 1 3 10 10 0
	# @./deadlock.sh 3 2 100 100 100
	# @./deadlock.sh 33 22 100 100 1000
	# @./deadlock.sh 100 100 100 100 1000
	
clean:
	rm -f proj2
