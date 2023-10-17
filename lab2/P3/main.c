#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    int i;
    pid_t child_pid, wpid;
    int status;
    for (i = 0; i < 3; i++)
    {
        child_pid = fork();
        if (child_pid == -1)
        {
           perror("fork");
           exit(1);
        }
        if (child_pid == 0)
        {
           int sleep_time = (i + 1) * 2;
           printf("The child process %d is waiting %d seconds\n", getpid(), sleep_time);
           sleep(sleep_time);
           printf("The child process %d finished!\n",getpid());
           exit(0);
        }
    }
    while ((wpid = waitpid(-1, &status, 0)) > 0)
    {
        if (WIFEXITED(status)) 
        {
            printf("The child process %d finished with status %d\n", wpid, WEXITSTATUS(status));
        }
        else {
            printf("The child process %d did not finished normally\n", wpid);
        }
    }
    printf("The program finished succesfully\n");
    return 0;
}