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

#define SHM_KEY 1303
#define SEM_KEY 2210

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

bool isTurnPlayer1 = true;  
bool signalStatus = true;   
int semaphoreId;            
int sharedMemoryId;         
int timeout;                

/*****************************************************************************************
*                                DICHIARAZIONE DELLE FUNZIONI                             *
******************************************************************************************/

void initializeBoard();        
void establishConnection();    
void handleError(const char *message);  
void modifySemaphore(int semid, unsigned short sem_num, short sem_op);  
void switchTurnAndToken(char token1, char token2);  
void placeToken();             
int checkGameStatus();         
void terminateServer();        
void signalHandler(int sig);   
void handleTimeout();          
void printError(const char *message, bool mode);  

/*****************************************************************************************
*                                         MAIN                                           *
******************************************************************************************/

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printError("Usage error: ./TrisServer <timeout> <token1> <token2>\n", false);
    }

    timeout = atoi(argv[1]);
    char token1 = *argv[2];
    char token2 = *argv[3];

    if (token1 == token2) {
        printError("Error: You have entered two identical tokens!\n", false);
    }

    establishConnection();  
    initializeBoard();      

    shared_memory->token = token1;  

    printf("Waiting for the first player to connect...\n");
    modifySemaphore(semaphoreId, 3, 1); 
    modifySemaphore(semaphoreId, 0, -1); 
    printf("First player has connected!\n");

    shared_memory->token = token1; 
    modifySemaphore(semaphoreId, 3, 1);
    printf("Waiting for the second player to connect...\n");
    modifySemaphore(semaphoreId, 0, -1);

    printf("Second player has connected! The game begins!\nWaiting for the first player's move...\n");

    int gameStatus = 0;

    while (gameStatus == 0) { 
        alarm(timeout);  
        modifySemaphore(semaphoreId, 0, -1);  
        alarm(0);  
        placeToken();  
        gameStatus = checkGameStatus();  
        if (gameStatus == 0) {
            switchTurnAndToken(token1, token2);  
        }
    } 

    if (gameStatus == 2) {
        printf("It's a draw!\n");
        shared_memory->move = -3;
    } else {
        if (isTurnPlayer1) {
            shared_memory->move = -1;
            printf("Player '%c' wins!\n", token1);
        } else {  
            shared_memory->move = -2;
            printf("Player '%c' wins!\n", token2);
        }
    }

    modifySemaphore(semaphoreId, 2, 1);
    modifySemaphore(semaphoreId, 1, 1);

    modifySemaphore(semaphoreId, 0, -1);
    terminateServer();    
}

/*****************************************************************************************
*                                INIZIALIZZAZIONE CAMPO                                  *
******************************************************************************************/

void initializeBoard() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            shared_memory->grid[i][j] = ' ';
        }
    }

    shared_memory->playerCount = 0;  
    if ((shared_memory->serverPid = getpid()) == -1) {
        handleError("Error: getpid failed!");
    }
}

/*****************************************************************************************
*                                 CONNESSIONE AL SERVER                                  *
******************************************************************************************/

void establishConnection() {
    sharedMemoryId = shmget(SHM_KEY, sizeof(struct GameBoard), IPC_EXCL | IPC_CREAT | 0666);
    if (sharedMemoryId == -1) {
        printError("Error: shmget failed!\n", true);
    }

    shared_memory = (struct GameBoard*)shmat(sharedMemoryId, NULL, 0);
    if (shared_memory == (struct GameBoard *)-1) {
        printError("Error: shmat failed!\n", true);
    }

    if (signal(SIGUSR2, signalHandler) == SIG_ERR || signal(SIGINT, signalHandler) == SIG_ERR || signal(SIGUSR1, signalHandler) == SIG_ERR || signal(SIGTERM, signalHandler) == SIG_ERR || signal(SIGALRM, signalHandler) == SIG_ERR) {
        printError("Error: Signal initialization failed!", true);
    }

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

/*****************************************************************************************
*                                  MODIFICA DEI SEMAFORI                                 *
******************************************************************************************/

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

/*****************************************************************************************
*                                CAMBIO TURNO E TOKEN                                   *
******************************************************************************************/

void switchTurnAndToken(char token1, char token2) {
    if (isTurnPlayer1) {
        modifySemaphore(semaphoreId, 2, 1);  
    } else {
        modifySemaphore(semaphoreId, 1, 1);  
    }
    isTurnPlayer1 = !isTurnPlayer1;  
    shared_memory->token = isTurnPlayer1 ? token1 : token2;  
}

/*****************************************************************************************
*                                 INSERIMENTO DEL TOKEN                                  *
******************************************************************************************/

void placeToken() {
    int pos = shared_memory->move;  
    int row = pos / 3;  
    int col = pos % 3;  

    if (shared_memory->grid[row][col] == ' ') {
        shared_memory->grid[row][col] = shared_memory->token;
        printf("Player '%c' made a move at [%d, %d]\n", shared_memory->token, row + 1, col + 1);
    } else {
        printf("Invalid move! The position is already taken.\n");
    }
}

/*****************************************************************************************
*                                 CONTROLLO DELLO STATO                                  *
******************************************************************************************/

int checkGameStatus() {

    // Controllo righe e colonne per una vittoria
    for (int i = 0; i < 3; i++) {
        if (shared_memory->grid[i][0] == shared_memory->token &&
            shared_memory->grid[i][1] == shared_memory->token &&
            shared_memory->grid[i][2] == shared_memory->token) {
            return 1;  // Vittoria
        }

        if (shared_memory->grid[0][i] == shared_memory->token &&
            shared_memory->grid[1][i] == shared_memory->token &&
            shared_memory->grid[2][i] == shared_memory->token) {
            return 1;  // Vittoria
        }
    }

    // Controllo le diagonali per una vittoria
    if ((shared_memory->grid[0][0] == shared_memory->token &&
         shared_memory->grid[1][1] == shared_memory->token &&
         shared_memory->grid[2][2] == shared_memory->token) ||
        (shared_memory->grid[0][2] == shared_memory->token &&
         shared_memory->grid[1][1] == shared_memory->token &&
         shared_memory->grid[2][0] == shared_memory->token)) {
        return 1;  // Vittoria
    }

    // Controllo se c'è ancora spazio per giocare
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (shared_memory->grid[i][j] == ' ') {
                return 0;  // Gioco ancora in corso
            }
        }
    }

    return 2;  // Pareggio
}

/*****************************************************************************************
*                               GESTORE DEI SEGNALI                                     *
******************************************************************************************/

void signalHandler(int sig) {
    switch (sig) { 
        case SIGALRM:
            handleTimeout();  
            break;
        case SIGTERM:
        case SIGINT:
            if (signalStatus) {
                printf("Press Ctrl + C again to terminate the game!\n");
                signalStatus = false;
            } else {
                if (shared_memory->playerCount == 0) {
                    printf("Forcing server shutdown!\n");
                    terminateServer();
                } else {
                    if (shared_memory->playerCount == 1) {
                        kill(shared_memory->player1, SIGUSR2);
                        printf("Forcing server shutdown!\n");
                        terminateServer();
                    } else {
                        kill(shared_memory->player1, SIGUSR2);
                        kill(shared_memory->player2, SIGUSR2);
                        printf("The game was forcibly terminated.\n");
                        terminateServer();
                    }
                }
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

/*****************************************************************************************
*                                TERMINAZIONE DEL SERVER                                *
******************************************************************************************/

void terminateServer() {
    if (shared_memory->playerCount > 0) {
        kill(shared_memory->player1, SIGUSR1); // Invia il segnale di terminazione al primo player
        if (shared_memory->playerCount > 1) {
            kill(shared_memory->player2, SIGUSR1); // Invia il segnale di terminazione al secondo player
        }
    }

    if (shmdt(shared_memory) == -1) {
        handleError("Error: shmdt failed!\n");
    }

    if (shmctl(sharedMemoryId, IPC_RMID, NULL) == -1)
        handleError("Error: shmctl failed!\n");

    if (semctl(semaphoreId, 0, IPC_RMID, 0) == -1)
        handleError("Error: semctl failed!\n");

    printf("Game over.\n");
    exit(EXIT_SUCCESS);
}



/*****************************************************************************************
*                                GESTIONE ERRORI                                        *
******************************************************************************************/

void printError(const char *message, bool mode) {
    if (mode) {
        perror(message);
        exit(EXIT_SUCCESS);
    } else {
        printf("%s", message);
        exit(EXIT_SUCCESS);
    }
}

void handleError(const char *message) {
    perror(message);
    kill(shared_memory->player1, SIGUSR2);
    kill(shared_memory->player2, SIGUSR2);
    terminateServer();
}

/*****************************************************************************************
*                              GESTIONE DEL TIMEOUT                                     *
******************************************************************************************/

void handleTimeout() {
    printf("Timeout! Player '%c' took too long. The other player wins by default.\n", shared_memory->token);
    if (isTurnPlayer1) {
        shared_memory->move = -2;  // Giocatore 2 vince
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