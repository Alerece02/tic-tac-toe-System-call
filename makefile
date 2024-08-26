# Makefile per il progetto TRIS

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Wno-implicit-function-declaration
TARGETS = TriServer TriClient
COMMON_SRC = common.c

all: $(TARGETS)

TriServer: TriServer.c $(COMMON_SRC) common.h
	$(CC) $(CFLAGS) -o TriServer TriServer.c $(COMMON_SRC)

TriClient: TriClient.c $(COMMON_SRC) common.h
	$(CC) $(CFLAGS) -o TriClient TriClient.c $(COMMON_SRC)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
