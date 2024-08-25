#include "common.h"

struct GameBoard *shared_memory;
struct semid_ds sem_info;

int semaphoreId, sharedMemoryId;
int player = 0;
bool noError = true;
bool isBot = false;

void initializeSemaphores();
void closeClient();
void makeMove();
void makeRandomMove();
void waitForTurn();
void playGame();
void printBoard();
void endGame();
void signalHandler(int sig);

int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        // Possibile aggiunta di codice per gestire gli argomenti in futuro
    }

    if (argc < 2 || argc > 3) {
        printError("Errore di utilizzo: ./TriClient <playerName> [BOTMODE]\n", false);
    }

    char playerName[MAX_PLAYER_NAME];
    strncpy(playerName, argv[1], MAX_PLAYER_NAME);

    if (strlen(playerName) > 100) {
        printError("Error: il nome usato è troppo lungo MAX CHARACTERS 30!\n", false);
    }

    if (argc == 3 && strcmp(argv[2], "*") == 0) {
        connectToSharedMemory(&shared_memory, &sharedMemoryId);
        if (shared_memory != NULL) {
            shared_memory->isBotMode = true; 
            printf("Client: Requesting server to start bot mode...\n");
        } else {
            printf("Client: Failed to connect to shared memory!\n");
        }
        closeClient();
    }

    if (argc == 3 && strcmp(argv[2], "BOTMODE") == 0) {
        isBot = true;
        printf("Client: Bot mode activated with BOTMODE keyword, PID: %d\n", getpid());
        srand(time(NULL));
    }

    registerSignal(SIGUSR2, signalHandler);
    registerSignal(SIGINT, signalHandler);
    registerSignal(SIGUSR1, signalHandler);
    registerSignal(SIGTERM, signalHandler);

    initializeSemaphores();
    printf("Hello %s! Welcome to Tic-Tac-Toe!\nWaiting for the server to connect...\n", playerName);

    modifySemaphore(semaphoreId, 3, -1);
    if (semctl(semaphoreId, 0, IPC_STAT, &sem_info) == -1) {
        printError("Error: semctl failed!", true); 
    }

    printf("The server is connected!\n");
    connectToSharedMemory(&shared_memory, &sharedMemoryId);
    shared_memory->playerCount++;

    if (shared_memory->playerCount > 2) {
        printf("The game is currently full! Please try again later!\n");
        closeClient();
    }

    fflush(stdout);

    if (shared_memory->playerCount == 1) {
        player = 1;
        shared_memory->player1 = getpid();
        printf("Your symbol is '%c'\nWaiting for another player to join...\n", shared_memory->symbol);
        modifySemaphore(semaphoreId, 0, 1);
        modifySemaphore(semaphoreId, 3, 1);
        modifySemaphore(semaphoreId, 1, -1);
        printf("A second player has joined!\nMake your first move!\n");
    } else { 
        player = 2;
        shared_memory->player2 = getpid();
        printf("Your symbol is '%c'\nThe game begins!\nWaiting for the first player's move...\n", shared_memory->symbol);
        printBoard();
        modifySemaphore(semaphoreId, 0, 1);
        modifySemaphore(semaphoreId, 1, 1);
        modifySemaphore(semaphoreId, 2, -1);
    }

    playGame();
    endGame();

    printf("Game over!\n");
    closeClient();
}

void playGame() {
    while (shared_memory->move >= 0) { 
        system("clear");
        printBoard();
        if (isBot) {
            makeRandomMove();
        } else {
            makeMove();
        }
        waitForTurn();
    }
}

void endGame() {
    system("clear");
    printBoard();

    if (shared_memory->move == -1) {
        if (player == 1)
            printf("Congratulations! You won!\n");
        else
            printf("You lost. Better luck next time!\n");
    } else if (shared_memory->move == -2) {
        if (player == 2) {
            printf("Congratulations! You won!\n");
        } else {
            printf("You lost. Better luck next time!\n");
        }
    } else {
        printf("The game ended in a draw!\n");
    }
}

void printBoard() {
    printf("\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf(" %c ", shared_memory->grid[i][j]);
            if (j < 2) printf("|");  // Linea verticale di separazione
        }
        printf("\n");
        if (i < 2) printf("---+---+---\n");  // Linea orizzontale di separazione
    }
    printf("\n");
}

void makeMove() {
    int pos;
    bool invalidMove = false;

    do {
        printf("Giocatore %d (%c), è il tuo turno!\n", player, shared_memory->symbol);
        printf("Inserisci la tua mossa (1-9), dove:\n");
        printf(" 1 | 2 | 3 \n---+---+---\n 4 | 5 | 6 \n---+---+---\n 7 | 8 | 9\n\n");
        printf("Posizione: ");
        
        if (scanf("%d", &pos) != 1) {
            invalidMove = true;
            while (getchar() != '\n');  // Pulisce il buffer
            printf("Input non valido! Per favore inserisci un numero tra 1 e 9.\n");
        } else {
            if (pos <= 0 || pos > 9) {
                printf("Mossa non valida! Inserisci un numero tra 1 e 9.\n");
                invalidMove = true;
            } else {
                int row = (pos - 1) / 3;
                int col = (pos - 1) % 3;
                if (shared_memory->grid[row][col] != ' ') {
                    printf("Questa posizione è già occupata. Scegline un'altra!\n");
                    invalidMove = true;
                } else {
                    shared_memory->move = pos - 1;
                    invalidMove = false;
                }
            }
        }
    } while (invalidMove);
}

void makeRandomMove() {
    int pos;
    bool invalidMove = false;

    printf("Bot is making a move...\n");
    do {
        invalidMove = false;
        pos = rand() % 9 + 1;
        int row = (pos - 1) / 3;
        int col = (pos - 1) % 3;
        
        if (shared_memory->grid[row][col] != ' ') {
            invalidMove = true;
        } else {
            shared_memory->move = pos - 1;
        }
    } while (invalidMove);

    printf("Bot placed a symbol at position %d\n", pos);
}

void waitForTurn() {
    system("clear");
    printf("Move made: %d\n", shared_memory->move + 1);
    printf("Waiting for the other player to make a move...\n");
    modifySemaphore(semaphoreId, 0, 1);
    modifySemaphore(semaphoreId, player, -1);
}

void initializeSemaphores() {
    semaphoreId = semget(SEM_KEY, 4, IPC_CREAT | 0666);
    if (semaphoreId == -1) {
        handleError("Error: semget failed!\n");
    }

    if (semctl(semaphoreId, 0, IPC_STAT, &sem_info) == -1) { 
        printError("Error: semctl failed!", true); 
    }
}

void closeClient() {
    modifySemaphore(semaphoreId, 3, -1);  // Lock

    if (shared_memory != NULL) {
        if (shared_memory->playerCount > 0) {
            shared_memory->playerCount--;
        }
    }

    // Non rilasciare il semaforo se il client è in modalità bot
    if (shared_memory != NULL && shared_memory->playerCount == 0 && noError) {
        modifySemaphore(semaphoreId, 0, 1);
    }

    modifySemaphore(semaphoreId, 3, 1);  // Unlock
    
    exit(EXIT_SUCCESS);
}

void signalHandler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
        case SIGUSR1:
            printf("\nThe game has ended. Exiting...\n");
            closeClient();
            break;
        case SIGUSR2:
            printf("\nThe game has ended. Exiting...\n");
            printf("\nYou lost. Better luck next time!\n");
            closeClient();
            break;
    }
}
