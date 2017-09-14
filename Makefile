.PHONY: run debug valgrind

CC      = gcc
CCFLAGS = -Wall -g
LDFLAGS = -lpthread

# Debug setup
run: all
	@./dragonbar

debug: all
	@gdb -q ./dragonbar

valgrind: all
	@ valgrind --leak-check=yes ./dragonbar

all: dragonbar

dragonbar: dragonbar.c
	@$(CC) $(CCFLAGS) $(LDFLAGS) dragonbar.c -o dragonbar
