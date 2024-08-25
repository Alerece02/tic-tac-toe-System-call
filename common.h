#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define SHM_KEY 1303
#define SEM_KEY 2210
#define MAX_PLAYER_NAME 30


struct GameBoard {
    char grid[3][3];
    char symbol;
    int playerCount;
    int move;
    pid_t player1;
    pid_t player2;
    pid_t serverPid;
    bool isBotMode;
};

// Dichiarazioni delle funzioni comuni
void modifySemaphore(int semid, unsigned short sem_num, short sem_op);
void handleError(const char *message);
void printError(const char *message, bool mode);
void connectToSharedMemory(struct GameBoard **shared_memory, int *sharedMemoryId);
void detachSharedMemory(struct GameBoard *shared_memory);
void registerSignal(int sig, void (*handler)(int));

#endif 
