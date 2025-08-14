# User-Space Makefile for IKOS IPC API

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../include
LDFLAGS = 

# Source files
SOURCES = ipc_user_api.c ipc_example.c
OBJECTS = $(SOURCES:.c=.o)

# Targets
EXAMPLE = ipc_example
LIBRARY = libipc_user.a

.PHONY: all clean example library

all: example library

example: $(EXAMPLE)

library: $(LIBRARY)

$(EXAMPLE): ipc_example.o ipc_user_api.o
	$(CC) $(LDFLAGS) -o $@ $^

$(LIBRARY): ipc_user_api.o
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(EXAMPLE) $(LIBRARY)

install: $(LIBRARY)
	cp $(LIBRARY) ../lib/
	cp ipc_user_api.h ../include/

# Test targets
test-server: $(EXAMPLE)
	./$(EXAMPLE) server

test-client: $(EXAMPLE)
	./$(EXAMPLE) client 1

test-broadcast: $(EXAMPLE)
	./$(EXAMPLE) broadcast

test-async: $(EXAMPLE)
	./$(EXAMPLE) async

# Dependencies
ipc_example.o: ipc_example.c ipc_user_api.h
ipc_user_api.o: ipc_user_api.c ipc_user_api.h
