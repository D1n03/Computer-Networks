#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

void sighandler(int sig)
{
    printf("Got signal %d\n", sig);
    fflush(stdout);
}

int main()
{
    signal(SIGABRT, sighandler);
    pid_t target_pid = getpid();
    int count = 0;
    while (1) {
        sleep(3);
        if (kill(target_pid, SIGABRT) == -1) {
           perror("kill");
           exit(1);
        }
        count++;
        printf("Sent SIGABRT signal to target to process (count = %d)\n", count);
    }
    return 0;
}