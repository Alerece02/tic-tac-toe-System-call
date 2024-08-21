#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
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

/*****************************************************************************************
*                                  STRUCTURE E VARIABILI GLOBALI                          *
******************************************************************************************/

struct GameBoard {
    char grid[3][3];        
    char token;             
    int playerCount;        
    int move;               
    pid_t player1;          
    pid_t player2;          
    pid_t serverPid;        
};

struct GameBoard *shared_memory;
struct semid_ds sem_info;

int semaphoreId, sharedMemoryId; 
int player = 0;                  
bool noError = true;             
bool isBot = false;  // Aggiunto per identificare se il client è un bot

/*****************************************************************************************
*                                DICHIARAZIONE DELLE FUNZIONI                             *
******************************************************************************************/

void connectToServer();  
void initializeSemaphores();  
void modifySemaphore(int semid, unsigned short sem_num, short sem_op);  
void closeClient();  
void makeMove();  
void makeRandomMove();  // Aggiunto per il bot
void waitForTurn();  
void playGame();  
void printBoard();  
void endGame();  
void signalHandler(int sig);  
void handleError(const char *message);  
void printError(const char *message, bool mode);  

/*****************************************************************************************
*                                         MAIN                                           *
******************************************************************************************/

int main(int argc, char *argv[]) {

    if (argc < 2 || argc > 3) {
        printError("Usage error: ./TrisClient <playerName> [*]\n", false);
    }

    char playerName[MAX_PLAYER_NAME];
    strncpy(playerName, argv[1], MAX_PLAYER_NAME);

    if (strlen(playerName) > 100) {
        printError("Error: Player name is too long! MAX CHARACTERS 100!\n", false);
    }

    if (argc == 3 && strcmp(argv[2], "*") == 0) {
        // Creazione di un processo figlio per il bot
        pid_t pid = fork();

        if (pid < 0) {
            handleError("Error: fork failed!\n");
        } else if (pid == 0) {
            // Processo figlio - Bot
            isBot = true;
            printf("Bot mode activated. Playing automatically...\n");
            srand(time(NULL));  // Inizializza il generatore di numeri casuali
        } else {
            // Processo padre - Continua come client normale
            //printf("Bot process started with PID: %d\n", pid);
            exit(EXIT_SUCCESS);  // Termina il processo padre dopo aver avviato il bot
        }
    }

    if (signal(SIGUSR2, signalHandler) == SIG_ERR || signal(SIGINT, signalHandler) == SIG_ERR || signal(SIGUSR1, signalHandler) == SIG_ERR || signal(SIGTERM, signalHandler) == SIG_ERR) {
        printError("Error: Signal initialization failed!\n", true);
    }

    initializeSemaphores();  
    printf("Hello %s! Welcome to Tic-Tac-Toe!\nWaiting for the server to connect...\n", playerName);

    modifySemaphore(semaphoreId, 3, -1);
    if (semctl(semaphoreId, 0, IPC_STAT, &sem_info) == -1) {
        printError("Error: semctl failed!", true); 
    }

    printf("The server is connected!\n");
    connectToServer();    
    shared_memory->playerCount++;    

    if (shared_memory->playerCount > 2) {
        printf("The game is currently full! Please try again later!\n");
        closeClient();
    }

    fflush(stdout);    

    if (shared_memory->playerCount == 1) {
        player = 1;
        if ((shared_memory->player1 = getpid()) == -1)
            handleError("Error: getpid failed!\n");
        printf("Your token is '%c'\nWaiting for another player to join...\n", shared_memory->token);
        modifySemaphore(semaphoreId, 0, 1);
        modifySemaphore(semaphoreId, 3, 1);
        modifySemaphore(semaphoreId, 1, -1);  
        printf("A second player has joined!\nMake your first move!\n");
    } else { 
        player = 2;
        if ((shared_memory->player2 = getpid()) == -1)
            handleError("Error: getpid failed!\n");
        printf("Your token is '%c'\nThe game begins!\nWaiting for the first player's move...\n", shared_memory->token);
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

/*****************************************************************************************
*                                     GESTIONE DEL GIOCO                                 *
******************************************************************************************/

void playGame() {
    while (shared_memory->move >= 0) { 
        system("clear");    
        printBoard();      

        if (isBot && ((player == 1 && shared_memory->token == 'X') || (player == 2 && shared_memory->token == 'O'))) {
            makeRandomMove();  // Il bot fa una mossa casuale
        } else {
            makeMove();  // Mossa manuale per il giocatore umano
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
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("[%c]", shared_memory->grid[i][j]); 
        }
        printf("\n");
    }
}

/*****************************************************************************************
*                                 GESTIONE DELLA MOSSA                                   *
******************************************************************************************/

void makeMove() {
    int pos;
    bool invalidMove = false;
    
    printf("It's your turn!\n");

    do {    
        invalidMove = false;    
        printf("Enter your move (1-9): ");
        if (scanf("%d", &pos) != 1) {
            invalidMove = true;
            while (getchar() != '\n');
            fflush(stdout);
            printf("Invalid input! Please enter a number between 1 and 9.\n");
        } else {
            if (pos <= 0 || pos > 9) {
                printf("Invalid move! Please enter a number between 1 and 9.\n");
                invalidMove = true;
            } else {
                int row = (pos - 1) / 3;
                int col = (pos - 1) % 3;
                if (shared_memory->grid[row][col] != ' ') {
                    printf("This position is already taken. Choose another one!\n");
                    invalidMove = true;
                }
            }
        }
        fflush(stdout);    
    } while (invalidMove);
    
    shared_memory->move = pos - 1;  
}

// Funzione per fare una mossa casuale (per il bot)
void makeRandomMove() {
    int pos;
    bool invalidMove = false;

    printf("Bot is making a move...\n");
    do {
        invalidMove = false;
        pos = rand() % 9 + 1;  // Genera un numero casuale tra 1 e 9
        int row = (pos - 1) / 3;
        int col = (pos - 1) % 3;
        
        if (shared_memory->grid[row][col] != ' ') {
            invalidMove = true;
        } else {
            shared_memory->move = pos - 1;  // Aggiorna la mossa effettiva nella shared memory
        }
    } while (invalidMove);

    printf("Bot placed a token at position %d\n", pos);

    // Lascia al server il compito di aggiornare la griglia
}

void waitForTurn() {
    system("clear");
    printf("Move made: %d\n", shared_memory->move + 1);
    printf("Waiting for the other player to make a move...\n");
    modifySemaphore(semaphoreId, 0, 1);
    modifySemaphore(semaphoreId, player, -1);
}

/*****************************************************************************************
*                               GESTIONE DELLE RISORSE E DEI SEGNALI                     *
******************************************************************************************/

void connectToServer() {
    sharedMemoryId = shmget(SHM_KEY, sizeof(struct GameBoard), IPC_CREAT | 0666);
    if (sharedMemoryId == -1) {
        printError("Error: shmget failed!\n", true);
    }

    shared_memory = (struct GameBoard*)shmat(sharedMemoryId, NULL, 0);
    if (shared_memory == (struct GameBoard *)-1) {
        printError("Error: shmat failed!\n", true);
    }    
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

void closeClient() {
    shared_memory->playerCount--;    

    if (shared_memory->playerCount == 0 && noError) { 
        if (shmdt(shared_memory) == -1) { 
            handleError("Error: shmdt failed!");
        }
        modifySemaphore(semaphoreId, 0, 1);
    } else {        
        if (shmdt(shared_memory) == -1) {
            handleError("Error: shmdt failed!");
        }    
    }

    modifySemaphore(semaphoreId, 3, 1);
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
            printf("\nYou lost. Better luck next time!\n");
            closeClient();
            break;
    }
}

/*****************************************************************************************
*                                    GESTIONE ERRORI                                     *
******************************************************************************************/

void printError(const char *message, bool mode) {
    if (mode) {
        perror(message);
        exit(EXIT_SUCCESS);
    } else {
        printf("%s\n", message);
        exit(EXIT_SUCCESS);
    }
}

void handleError(const char *message) { 
    perror(message);

    if (player == 1) {
        kill(shared_memory->serverPid, SIGUSR1);
    } else {
        kill(shared_memory->serverPid, SIGUSR2);
    }

    closeClient();
}