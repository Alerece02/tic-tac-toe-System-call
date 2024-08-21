# Makefile per il progetto TRIS

# Variabili
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = TriServer TriClient

# Regole di compilazione
all: $(TARGETS)

TriServer: TriServer.c
	$(CC) $(CFLAGS) -o TriServer TriServer.c

TriClient: TriClient.c
	$(CC) $(CFLAGS) -o TriClient TriClient.c

# Regole di pulizia
clean:
	rm -f $(TARGETS)

.PHONY: all clean