CC = gcc
CFLAGS = -Wall -g -DDEBUG
SRC = src
TESTS = test
FILES = $(SRC)/comm.c $(SRC)/log.c
CMD = $(CC) $(CFLAGS) $(FILES) -lpthread -Iinclude

all: comm_test

client:
	$(CMD) $(SRC)/server.c -o client

comm_test:
	$(CMD) $(TESTS)/comm_test.c -o comm_test

sync_test:
	$(CMD) $(TESTS)/sync_test.c -o sync_test

clean:
	rm -f client comm_test sync_test
