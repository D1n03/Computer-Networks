#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() 
{
    pid_t child_pid;
    child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) { // child 
        printf("Before exec. PID: %d\n", getpid());
        execlp("ls", "ls", NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // parent
        int status;
        waitpid(child_pid, &status, 0);
        printf("After exec. PID: %d\n", getpid());
    }
    return 0;
}
