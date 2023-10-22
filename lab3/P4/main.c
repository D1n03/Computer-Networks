#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() 
{
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Error creating pipe");
        exit(1);
    }
    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("Error creating child process");
        exit(1);
    }

    if (child_pid == 0) 
    { // child
        close(pipe_fd[0]);
        dup2(pipe_fd[1], 1);
        execlp("ls", "ls", "-l", "/", NULL);
        perror("Error at exec() in the child process");
        exit(1);
    } else { // parent
        close(pipe_fd[1]);
        dup2(pipe_fd[0], 0);
        char buffer[4096];
        ssize_t bytes_read;

        while ((bytes_read = read(0, buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytes_read);
        }

        int status;
        waitpid(child_pid, &status, 0);
    }
    return 0;
}
