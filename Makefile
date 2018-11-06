CC = gcc
CFLAGS = -Wall -g #-DDEBUG
SRC = src
TESTS = test
FILES = $(SRC)/comm.c $(SRC)/sync.c $(SRC)/log.c $(SRC)/file.c
CMD = $(CC) $(CFLAGS) $(FILES) -lpthread -Iinclude -lm

all: server

server:
	$(CMD) $(SRC)/server.c -o server

clean:
	rm -f client server
