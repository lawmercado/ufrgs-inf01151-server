CC = gcc
CFLAGS = -Wall -g -DDEBUG
SRC = src
TESTS = test
FILES = $(SRC)/comm.c $(SRC)/log.c
CMD = $(CC) $(CFLAGS) $(FILES) -lpthread -Iinclude

all: server

server:
	$(CMD) $(SRC)/server.c -o server

clean:
	rm -f client server
