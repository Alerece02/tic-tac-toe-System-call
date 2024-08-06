#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define N 3

typedef struct CampoCondiviso{
    
    char campo[N][N];
    char turno;
    int timeout;

}CampoCondiviso;

CampoCondiviso *campoCondiviso;

void stampaCampo();

int main(int argc, char *argv[]){

    if(argc != 2){
        perror("Non hai inserito il numero corretto di parametri!\n");
        exit(0);
    }

    


    return 0;
}


void stampaCampo(){

    for (int i = 0; i < N; i++){
        for (int j = 0; j < N; j++){
            printf(" %c ", campoCondiviso->campo[i][j]);
            if (j < N - 1){
                printf("|");
            }
        }

        printf("\n");
        if (i < N - 1){
            printf("---|---|---\n");
        }
    }
}