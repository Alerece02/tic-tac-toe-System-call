#include "common.h"

void modifySemaphore(int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {.sem_num = sem_num, .sem_op = sem_op, .sem_flg = 0};
    bool interrupted = false;

    while (semop(semid, &sop, 1) == -1 && errno == EINTR) {
        interrupted = true;
    }

    if (errno != EINTR && interrupted) {
        handleError("Error: semop failed");
    }
}

void handleError(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void printError(const char *message, bool mode) {
    if (mode) {
        perror(message);
        exit(EXIT_FAILURE);
    } else {
        printf("%s", message);
        exit(EXIT_FAILURE);
    }
}

void connectToSharedMemory(struct GameBoard **shared_memory, int *sharedMemoryId) {
    *sharedMemoryId = shmget(SHM_KEY, sizeof(struct GameBoard), IPC_CREAT | 0666);
    if (*sharedMemoryId == -1) {
        printError("Error: shmget failed!\n", true);
    }

    *shared_memory = (struct GameBoard*)shmat(*sharedMemoryId, NULL, 0);
    if (*shared_memory == (struct GameBoard *)-1) {
        printError("Error: shmat failed!\n", true);
    }
}

void detachSharedMemory(struct GameBoard *shared_memory) {
    if (shmdt(shared_memory) == -1) {
        handleError("Error: shmdt failed!\n");
    }
}

void registerSignal(int sig, void (*handler)(int)) {
    if (signal(sig, handler) == SIG_ERR) {
        handleError("Error: Signal registration failed");
    }
}
