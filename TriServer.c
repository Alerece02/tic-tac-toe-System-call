#include "common.h"

struct GameBoard *shared_memory;
bool isTurnPlayer1 = true;
bool signalStatus = true;
int semaphoreId;
int sharedMemoryId;
int timeout;

void initializeBoard();
void connection();
void switchTurn(char symbol1, char symbol2);
void placeSymbol();
int checkGameStatus();
void terminateServer();
void handleTimeout();
void handleBotMode();
void signalHandler(int sig);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printError("Usage error: ./TrisServer <timeout> <symbol1> <symbol2>\n", false);
    }

    timeout = atoi(argv[1]);
    char symbol1 = *argv[2];
    char symbol2 = *argv[3];

    if (symbol1 == symbol2) {
        printError("Error: You have entered two identical symbols!\n", false);
    }

    connection();  
    initializeBoard();      

    shared_memory->symbol = symbol1;  
    shared_memory->isBotMode = false;

    printf("Waiting for the first player to connect...\n");
    modifySemaphore(semaphoreId, 3, 1);
    modifySemaphore(semaphoreId, 0, -1);
    printf("First player has connected!\n");

    if (shared_memory->isBotMode) {
        handleBotMode();
    }

    shared_memory->symbol = symbol1; 
    modifySemaphore(semaphoreId, 3, 1);
    printf("Waiting for the second player to connect...\n");
    modifySemaphore(semaphoreId, 0, -1);

    printf("Second player has connected! The game begins!\nWaiting for the first player's move...\n");

    int gameStatus = 0;

    while (gameStatus == 0) { 
        alarm(timeout);  
        modifySemaphore(semaphoreId, 0, -1);  
        alarm(0);  
        placeSymbol();  
        gameStatus = checkGameStatus();  
        if (gameStatus == 0) {
            switchTurn(symbol1, symbol2);  
        }
    } 

    if (gameStatus == 2) {
        printf("It's a draw!\n");
        shared_memory->move = -3;
    } else {
        if (isTurnPlayer1) {
            shared_memory->move = -1;
            printf("Player '%c' wins!\n", symbol1);
        } else {  
            shared_memory->move = -2;
            printf("Player '%c' wins!\n", symbol2);
        }
    }

    modifySemaphore(semaphoreId, 2, 1);
    modifySemaphore(semaphoreId, 1, 1);

    modifySemaphore(semaphoreId, 0, -1);
    terminateServer();    
}

void initializeBoard() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            shared_memory->grid[i][j] = ' ';
        }
    }

    shared_memory->playerCount = 0;  
    shared_memory->serverPid = getpid();
}

void connection() {
    connectToSharedMemory(&shared_memory, &sharedMemoryId);

    registerSignal(SIGUSR2, signalHandler);
    registerSignal(SIGINT, signalHandler);
    registerSignal(SIGUSR1, signalHandler);
    registerSignal(SIGTERM, signalHandler);
    registerSignal(SIGALRM, signalHandler);

    unsigned short semInitVal[] = {0, 0, 0, 0};
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg;
    arg.array = semInitVal;
    semaphoreId = semget(SEM_KEY, 4, IPC_CREAT | 0666);
    if (semaphoreId == -1)
        handleError("Error: semget failed!");

    if (semctl(semaphoreId, 0, SETALL, arg) == -1)
        handleError("Error: semctl failed!");
}

void switchTurn(char symbol1, char symbol2) {
    if (isTurnPlayer1) {
        modifySemaphore(semaphoreId, 2, 1);
    } else {
        modifySemaphore(semaphoreId, 1, 1);
    }
    isTurnPlayer1 = !isTurnPlayer1;
    shared_memory->symbol = isTurnPlayer1 ? symbol1 : symbol2;
}

void placeSymbol() {
    int pos = shared_memory->move;  
    int row = pos / 3;  
    int col = pos % 3;  

    if (shared_memory->grid[row][col] == ' ') {
        shared_memory->grid[row][col] = shared_memory->symbol;
        printf("Player '%c' made a move at [%d, %d]\n", shared_memory->symbol, row + 1, col + 1);
    } else {
        printf("Invalid move! The position is already taken.\n");
    }
}

int checkGameStatus() {
    for (int i = 0; i < 3; i++) {
        if (shared_memory->grid[i][0] == shared_memory->symbol &&
            shared_memory->grid[i][1] == shared_memory->symbol &&
            shared_memory->grid[i][2] == shared_memory->symbol) {
            return 1;
        }

        if (shared_memory->grid[0][i] == shared_memory->symbol &&
            shared_memory->grid[1][i] == shared_memory->symbol &&
            shared_memory->grid[2][i] == shared_memory->symbol) {
            return 1;
        }
    }

    if ((shared_memory->grid[0][0] == shared_memory->symbol &&
         shared_memory->grid[1][1] == shared_memory->symbol &&
         shared_memory->grid[2][2] == shared_memory->symbol) ||
        (shared_memory->grid[0][2] == shared_memory->symbol &&
         shared_memory->grid[1][1] == shared_memory->symbol &&
         shared_memory->grid[2][0] == shared_memory->symbol)) {
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (shared_memory->grid[i][j] == ' ') {
                return 0;
            }
        }
    }

    return 2;
}

void terminateServer() {
    if (shared_memory->playerCount > 0) {
        kill(shared_memory->player1, SIGUSR1);
        if (shared_memory->playerCount > 1) {
            kill(shared_memory->player2, SIGUSR1);
        }
    }

    detachSharedMemory(shared_memory);

    if (shmctl(sharedMemoryId, IPC_RMID, NULL) == -1)
        handleError("Error: shmctl failed!\n");

    if (semctl(semaphoreId, 0, IPC_RMID, 0) == -1)
        handleError("Error: semctl failed!\n");

    printf("Game over.\n");
    exit(EXIT_SUCCESS);
}

void handleTimeout() {
    printf("Timeout! Player '%c' took too long. The other player wins by default.\n", shared_memory->symbol);
    if (isTurnPlayer1) {
        shared_memory->move = -2;
        kill(shared_memory->player2, SIGUSR1);
        kill(shared_memory->player1, SIGUSR2);
    } else {
        shared_memory->move = -1;
        kill(shared_memory->player1, SIGUSR1);
        kill(shared_memory->player2, SIGUSR2);
    }
    modifySemaphore(semaphoreId, 2, 1);
    modifySemaphore(semaphoreId, 1, 1);
    terminateServer();
}


void signalHandler(int sig) {
    
    registerSignal(SIGINT, signalHandler);

    switch (sig) { 
        case SIGALRM:
            handleTimeout();
            break;
        case SIGTERM:
        case SIGINT:
            if (signalStatus) {
                printf("Press Ctrl + C again to terminate the game!\n");
                signalStatus = false;
            } else if (shared_memory->playerCount == 0) {
                printf("Forcing server shutdown!\n");
                terminateServer();

                } else if (shared_memory->playerCount == 1) {
                    kill(shared_memory->player1, SIGUSR2);
                    printf("Forcing server shutdown!\n");
                    terminateServer();

                    } else {
                        kill(shared_memory->player1, SIGUSR2);
                        kill(shared_memory->player2, SIGUSR2);
                        printf("The game was forcibly terminated.\n");
                        terminateServer();
                    }     
            break;

        case SIGUSR1:
            if (shared_memory->playerCount == 0) {
                terminateServer();
            } else {
                kill(shared_memory->player2, SIGUSR1);
                terminateServer();
            }
            break;
        case SIGUSR2:
            if (shared_memory->playerCount == 0) {
                terminateServer();
            } else {
                kill(shared_memory->player1, SIGUSR1);
                terminateServer();
            }
            break;
    }
}

void handleBotMode() {
    
    if (shared_memory->isBotMode) {
        
        switch (fork()) {
            case -1:
                perror("Error: couldn't create the bot process");
                exit(EXIT_FAILURE);
            case 0: {
                printf("Server (child): Fork successful, starting bot mode with exec, PID: %d\n", getpid());
                char *args[] = {"./TriClient", "Bot", "BOTMODE", NULL};
                execvp(args[0], args);
                perror("Error: exec failed");
                exit(EXIT_FAILURE);
            }
            default:
                
                shared_memory->isBotMode = false;
                break;
        }
    }
}
