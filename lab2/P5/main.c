#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

void sighandler(int sig) {
    printf("Got signal!\n");
    fflush(stdout);
}

int main() 
{
    printf("%d\n", getpid());
    signal(SIGUSR1, sighandler);
    int pid;
    if(( pid = fork()) == -1) {
        perror("Eroare la fork()!");
        exit(1);
    }
    if( pid == 0) { // child process
      kill(getppid(), SIGUSR1);
    } else {
      while(1);
   }
}