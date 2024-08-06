# Definizione dei nomi dei file sorgente
SRCS_SERVER = TriServer.c
SRCS_CLIENT = TriClient.c

# Definizione dei nomi degli eseguibili
TARGET_SERVER = TriServer
TARGET_CLIENT = TriClient

# Opzioni del compilatore
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Regola per costruire l'eseguibile del server
$(TARGET_SERVER): $(SRCS_SERVER)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SRCS_SERVER)

# Regola per costruire l'eseguibile del client
$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(SRCS_CLIENT)

# Regola di pulizia
clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)
