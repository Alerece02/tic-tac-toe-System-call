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

// Definizione delle chiavi univoche per la memoria condivisa e per i semafori
#define SHM_KEY 1303
#define SEM_KEY 2210

/*****************************************************************************************
*                                  STRUCTURE E VARIABILI GLOBALI                          *
******************************************************************************************/

// Struttura che rappresenta lo stato del gioco e la comunicazione tra processi
struct GameBoard {
    char grid[3][3];        // La griglia di gioco 3x3
    char token;             // Il token corrente del giocatore ('X' o 'O')
    int playerCount;        // Numero di giocatori connessi al server
    int move;               // Ultima mossa effettuata
    pid_t player1;          // PID del primo giocatore
    pid_t player2;          // PID del secondo giocatore
    pid_t serverPid;        // PID del server
};

// Puntatore alla memoria condivisa
struct GameBoard *shared_memory;

bool isTurnPlayer1 = true;  // Indica se è il turno del primo giocatore
bool signalStatus = true;   // Controlla lo stato del segnale
int semaphoreId;            // ID del set di semafori
int sharedMemoryId;         // ID della memoria condivisa

/*****************************************************************************************
*                                DICHIARAZIONE DELLE FUNZIONI                             *
******************************************************************************************/

void initializeBoard();        // Inizializza la griglia di gioco
void establishConnection();    // Stabilisce la connessione alla memoria condivisa e ai semafori
void handleError(const char *message);  // Gestisce gli errori
void modifySemaphore(int semid, unsigned short sem_num, short sem_op);  // Modifica lo stato di un semaforo
void switchTurnAndToken(char token1, char token2);  // Cambia il turno e il token
void placeToken();             // Posiziona il token sulla griglia
int checkGameStatus();         // Controlla lo stato del gioco (vittoria, pareggio)
void terminateServer();        // Termina il server e rilascia le risorse
void signalHandler(int sig);   // Gestisce i segnali
void printError(const char *message, bool mode);  // Stampa errori

/*****************************************************************************************
*                                         MAIN                                           *
******************************************************************************************/

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printError("Usage error: ./TrisServer <token1> <token2>\n", false);
    }

    char token1 = *argv[1];
    char token2 = *argv[2];

    if (token1 == token2) {
        printError("Error: You have entered two identical tokens!\n", false);
    }

    establishConnection();  // Stabilisce la connessione con la memoria condivisa e i semafori
    initializeBoard();      // Inizializza la griglia di gioco

    shared_memory->token = token1;  // Imposta il primo token

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

    // Ciclo principale del gioco
    while (gameStatus == 0) { 
        modifySemaphore(semaphoreId, 0, -1);  // Attende una mossa
        placeToken();  
        gameStatus = checkGameStatus();  // Controlla lo stato del gioco
        if (gameStatus == 0) {
            switchTurnAndToken(token1, token2);  // Cambia il turno
        }
    } 

    // Gestisce la fine del gioco
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
    // Inizializza la griglia di gioco con spazi vuoti
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            shared_memory->grid[i][j] = ' ';
        }
    }

    shared_memory->playerCount = 0;  // Inizializza il numero di giocatori a 0
    if ((shared_memory->serverPid = getpid()) == -1) {
        handleError("Error: getpid failed!");
    }
}

/*****************************************************************************************
*                                 CONNESSIONE AL SERVER                                  *
******************************************************************************************/

void establishConnection() {
    // Creazione della memoria condivisa per gestire lo stato del gioco
    sharedMemoryId = shmget(SHM_KEY, sizeof(struct GameBoard), IPC_EXCL | IPC_CREAT | 0666);
    if (sharedMemoryId == -1) {
        printError("Error: shmget failed!\n", true);
    }

    // Collegamento alla memoria condivisa
    shared_memory = (struct GameBoard*)shmat(sharedMemoryId, NULL, 0);
    if (shared_memory == (struct GameBoard *)-1) {
        printError("Error: shmat failed!\n", true);
    }

    // Gestione dei segnali per interrompere il gioco in caso di necessità
    if (signal(SIGUSR2, signalHandler) == SIG_ERR || signal(SIGINT, signalHandler) == SIG_ERR || signal(SIGUSR1, signalHandler) == SIG_ERR || signal(SIGTERM, signalHandler) == SIG_ERR) {
        printError("Error: Signal initialization failed!", true);
    }

    // Creazione del set di semafori per la sincronizzazione tra processi
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

    // Inizializza i semafori con i valori iniziali
    if (semctl(semaphoreId, 0, SETALL, arg) == -1)
        handleError("Error: semctl failed!");
}

/*****************************************************************************************
*                                  MODIFICA DEI SEMAFORI                                 *
******************************************************************************************/

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

    // Effettua l'operazione sul semaforo gestendo eventuali interruzioni (errno == EINTR)
    while (semop(semid, &sop, 1) == -1 && errno == EINTR) {
        interrupted = true;
    }

    // Se l'errore non è stato un'interruzione, gestisce l'errore
    if (errno != EINTR && interrupted) {
        handleError("Error: semop failed");
    }
}

/*****************************************************************************************
*                                CAMBIO TURNO E TOKEN                                   *
******************************************************************************************/

/**
 * @brief Cambia il turno e il token tra i due giocatori
 * 
 * @param token1 Il token del primo giocatore
 * @param token2 Il token del secondo giocatore
 */
void switchTurnAndToken(char token1, char token2) {
    if (isTurnPlayer1) {
        modifySemaphore(semaphoreId, 2, 1);  // Sblocca il secondo giocatore
    } else {
        modifySemaphore(semaphoreId, 1, 1);  // Sblocca il primo giocatore
    }
    isTurnPlayer1 = !isTurnPlayer1;  // Cambia il turno
    shared_memory->token = isTurnPlayer1 ? token1 : token2;  // Imposta il token corretto per il prossimo turno
}

/*****************************************************************************************
*                                 INSERIMENTO DEL TOKEN                                  *
******************************************************************************************/

/**
 * @brief Posiziona il token sulla griglia in base alla mossa effettuata
 */
void placeToken() {
    int pos = shared_memory->move;  // Ottiene la posizione della mossa
    int row = pos / 3;  // Calcola la riga
    int col = pos % 3;  // Calcola la colonna

    // Controlla se la posizione è valida (non occupata) e piazza il token
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

/**
 * @brief Controlla se c'è un vincitore o se la partita è un pareggio
 * 
 * @return 1 se c'è un vincitore, 2 se è un pareggio, 0 se la partita continua
 */
int checkGameStatus() {

    bool isDraw = true;  // Assume che la partita sia un pareggio

    // Controlla se ci sono ancora spazi vuoti sulla griglia
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (shared_memory->grid[i][j] == ' ') {
                isDraw = false;  // Non è un pareggio se c'è almeno un spazio vuoto
            }
        }
    }

    // Se è un pareggio, restituisce 2
    if (isDraw) {
        return 2; 
    } else {
        // Controllo delle righe per determinare una vittoria
        for (int i = 0; i < 3; i++) {
            if (shared_memory->grid[i][0] == shared_memory->token &&
                shared_memory->grid[i][1] == shared_memory->token &&
                shared_memory->grid[i][2] == shared_memory->token) {
                return 1;  // C'è un vincitore
            }
        }

        // Controllo delle colonne per determinare una vittoria
        for (int j = 0; j < 3; j++) {
            if (shared_memory->grid[0][j] == shared_memory->token &&
                shared_memory->grid[1][j] == shared_memory->token &&
                shared_memory->grid[2][j] == shared_memory->token) {
                return 1;  // C'è un vincitore
            }
        }

        // Controllo delle diagonali per determinare una vittoria
        if (shared_memory->grid[0][0] == shared_memory->token &&
            shared_memory->grid[1][1] == shared_memory->token &&
            shared_memory->grid[2][2] == shared_memory->token) {
            return 1;  // C'è un vincitore
        }

        if (shared_memory->grid[0][2] == shared_memory->token &&
            shared_memory->grid[1][1] == shared_memory->token &&
            shared_memory->grid[2][0] == shared_memory->token) {
            return 1;  // C'è un vincitore
        }
    }

    return 0;  // La partita continua
}

/*****************************************************************************************
*                               GESTORE DEI SEGNALI                                     *
******************************************************************************************/

/**
 * @brief Gestisce i segnali per interrompere o terminare il gioco
 * 
 * @param sig Il segnale ricevuto
 */
void signalHandler(int sig) {
    switch (sig) { 
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
    // Distacco della memoria condivisa
    if (shmdt(shared_memory) == -1) {
        handleError("Error: shmdt failed!\n");
    }

    // Rimozione della memoria condivisa
    if (shmctl(sharedMemoryId, IPC_RMID, NULL) == -1)
        handleError("Error: shmctl failed!\n");

    // Rimozione dei semafori
    if (semctl(semaphoreId, 0, IPC_RMID, 0) == -1)
        handleError("Error: semctl failed!\n");

    printf("Game over.\n");
    exit(EXIT_SUCCESS);
}

/*****************************************************************************************
*                                GESTIONE ERRORI                                        *
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
        printf("%s", message);
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief Gestisce errori critici terminando il server
 * 
 * @param message Messaggio di errore
 */
void handleError(const char *message) {
    perror(message);
    kill(shared_memory->player1, SIGUSR2);
    kill(shared_memory->player2, SIGUSR2);
    terminateServer();
}
