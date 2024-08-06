// ========================================
// HEADER
// ========================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define N 3

// ========================================
// STRUCT FOR THE SHARED MEMORY
// ========================================
typedef struct CampoCondiviso{
    
    char campo[N][N];
    char turno;
    int timeout;

}CampoCondiviso;

CampoCondiviso *campoCondiviso;


// ========================================
// FUNCTIONS
// ========================================




// ========================================
// MAIN
// ========================================
int main(int argc, char *argv[]){

    //checking that the number of parameters is correct
    if(argc != 4){
        perror("Numero di parametri non valido!\n");
        exit(0);
    }

    //checking that the two signs are not the same.
    if(argv[2] == argv[3]){
        perror("Non ci possono essere due segni uguali!");
        exit(0);
    }

    //checking that the timer is not negative
    if(atoi(argv[1]) < 0){
        perror("Il timer non può essere negativo!");
        exit(0);
    }

    //storing the passed parameters
    int timer = atoi(argv[1]);
    char segno1 = *argv[2];
    char segno2 = *argv[3];


    return 0;
}


