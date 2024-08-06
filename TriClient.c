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

int main(int argc, char *argv[]){

    if(argc != 2){
        perror("Non hai inserito il numero corretto di parametri!\n");
        exit(0);
    }

    


    return 0;
}