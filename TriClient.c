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

#define SHM_KEY 1303
#define SEM_KEY 2210
#define MAX_PLAYER_NAME 30

/*****************************************************************************************
*                                  STRUCTURE E VARIABILI GLOBALI                          *
******************************************************************************************/

// Struttura che rappresenta lo stato del gioco e la comunicazione tra processi
struct GameBoard {
    char grid[3][3];        // La griglia di gioco 3x3
    char token;             // Il token corrente del giocatore ('X' o 'O')
    int playerCount;        // Numero di giocatori connessi
    int move;               // Ultima mossa effettuata
    pid_t player1;          // PID del primo giocatore
    pid_t player2;          // PID del secondo giocatore
    pid_t serverPid;        // PID del server
};

// Puntatore alla memoria condivisa
struct GameBoard *shared_memory;
struct semid_ds sem_info;

int semaphoreId, sharedMemoryId; // ID del set di semafori e della memoria condivisa
int player = 0;                  // Identifica se il giocatore è il primo o il secondo
bool noError = true;             // Indica se non ci sono stati errori critici

/*****************************************************************************************
*                                DICHIARAZIONE DELLE FUNZIONI                             *
******************************************************************************************/

void connectToServer();  // Connessione alla memoria condivisa
void initializeSemaphores();  // Inizializza i semafori
void modifySemaphore(int semid, unsigned short sem_num, short sem_op);  // Modifica lo stato di un semaforo
void closeClient();  // Chiude il client e rilascia le risorse
void makeMove();  // Permette al giocatore di fare una mossa
void waitForTurn();  // Attende il turno dell'altro giocatore
void playGame();  // Gestisce il ciclo di gioco
void printBoard();  // Stampa la griglia di gioco
void endGame();  // Gestisce la fine del gioco
void signalHandler(int sig);  // Gestisce i segnali
void handleError(const char *message);  // Gestisce gli errori
void printError(const char *message, bool mode);  // Stampa errori

/*****************************************************************************************
*                                         MAIN                                           *
******************************************************************************************/

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printError("Usage error: ./TrisClient <playerName>\n", false);
    }

    char playerName[MAX_PLAYER_NAME];
    strncpy(playerName, argv[1], MAX_PLAYER_NAME);
    
    if (strlen(playerName) > 100) {
        printError("Error: Player name is too long! MAX CHARACTERS 100!\n", false);
    }

    // Gestione dei segnali
    if (signal(SIGUSR2, signalHandler) == SIG_ERR || signal(SIGINT, signalHandler) == SIG_ERR || signal(SIGUSR1, signalHandler) == SIG_ERR || signal(SIGTERM, signalHandler) == SIG_ERR) {
        printError("Error: Signal initialization failed!\n", true);
    }

    initializeSemaphores();  // Inizializza i semafori per la sincronizzazione
    printf("Hello %s! Welcome to Tic-Tac-Toe!\nWaiting for the server to connect...\n", playerName);

    // Attende che il server si connetta
    modifySemaphore(semaphoreId, 3, -1);
    if (semctl(semaphoreId, 0, IPC_STAT, &sem_info) == -1) {
        printError("Error: semctl failed!", true); 
    }

    printf("The server is connected!\n");
    connectToServer();    
    shared_memory->playerCount++;    

    // Controlla se il numero massimo di giocatori è stato raggiunto
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
        modifySemaphore(semaphoreId, 1, -1);  // Attende che il giocatore 2 si unisca
        printf("A second player has joined!\nMake your first move!\n");
    } else { 
        player = 2;
        if ((shared_memory->player2 = getpid()) == -1)
            handleError("Error: getpid failed!\n");
        printf("Your token is '%c'\nThe game begins!\nWaiting for the first player's move...\n", shared_memory->token);
        printBoard();
        modifySemaphore(semaphoreId, 0, 1);
        modifySemaphore(semaphoreId, 1, 1);  // Sblocca il giocatore 1
        modifySemaphore(semaphoreId, 2, -1);  // Attende la mossa del giocatore 1
    }
    
    playGame();  // Avvia il gioco
    endGame();  // Gestisce la fine del gioco
        
    printf("Game over!\n");
    closeClient();
}

/*****************************************************************************************
*                                     GESTIONE DEL GIOCO                                 *
******************************************************************************************/

/**
 * @brief Gestisce il ciclo principale del gioco
 */
void playGame() {
    // Ciclo principale del gioco
    while (shared_memory->move >= 0) { 
        system("clear");    
        printBoard();      
        makeMove();         // Il giocatore effettua una mossa
        waitForTurn();      // Attende il turno dell'altro giocatore
    }
}

/**
 * @brief Gestisce la fine del gioco mostrando il risultato
 */
void endGame() {
    system("clear");
    printBoard();

    // Determina il risultato del gioco
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

/**
 * @brief Stampa la griglia di gioco aggiornata
 */
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

/**
 * @brief Permette al giocatore di inserire una mossa valida
 */
void makeMove() {
    int pos;
    bool invalidMove = false;
    
    printf("It's your turn!\n");

    // Richiede al giocatore di inserire la mossa fino a che non è valida
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
    
    shared_memory->move = pos - 1;  // Aggiorna la mossa nella memoria condivisa
}

/**
 * @brief Attende il turno dell'altro giocatore
 */
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

/**
 * @brief Connessione alla memoria condivisa
 */
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

/**
 * @brief Inizializza i semafori per la sincronizzazione
 */
void initializeSemaphores() {
    semaphoreId = semget(SEM_KEY, 4, IPC_CREAT | 0666);
    if (semaphoreId == -1) {
        handleError("Error: semget failed!\n");
    }

    if (semctl(semaphoreId, 0, IPC_STAT, &sem_info) == -1) { 
        printError("Error: semctl failed!", true); 
    }           
}

/**
 * @brief Modifica il valore di un semaforo all'interno di un set
 * 
 * @param semid ID del set di semafori
 * @param sem_num Indice del semaforo nel set
 * @param sem_op Operazione da effettuare sul semaforo (es. -1 per wait, 1 per signal)
 */
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

/**
 * @brief Chiude il client e rilascia le risorse
 */
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

/**
 * @brief Gestisce i segnali per interrompere o terminare il gioco
 * 
 * @param sig Il segnale ricevuto
 */
void signalHandler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            if (player == 1) {
                printf("\nYou have left the game.\n");
                kill(shared_memory->serverPid, SIGUSR1);      
                closeClient();
            } else if (player == 2) {
                printf("\nYou have left the game.\n");
                kill(shared_memory->serverPid, SIGUSR2);
                closeClient();
            } else {            
                if (semctl(semaphoreId, 0, IPC_RMID, 0) == -1) {
                    handleError("Error: semctl failed!\n");
                }
                exit(EXIT_SUCCESS);
            }
            break;
        case SIGUSR1:
            printf("\nThe opponent has left the game.\n You win!\n");
            closeClient();
            break;
        case SIGUSR2:
            printf("\nThe game was terminated by an external cause.\n");
            closeClient();
            break;
    }
}

/*****************************************************************************************
*                                    GESTIONE ERRORI                                     *
******************************************************************************************/

/**
 * @brief Stampa un errore e termina il programma
 * 
 * @param message Messaggio di errore
 * @param mode Se true, usa perror; se false, usa printf
 */
void printError(const char *message, bool mode) {
    if (mode) {
        perror(message);
        exit(EXIT_SUCCESS);
    } else {
        printf("%s\n", message);
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief Gestisce errori critici terminando il client
 * 
 * @param message Messaggio di errore
 */
void handleError(const char *message) { 
    perror(message);

    if (player == 1) {
        kill(shared_memory->serverPid, SIGUSR1);
    } else {
        kill(shared_memory->serverPid, SIGUSR2);
    }

    closeClient();
}
