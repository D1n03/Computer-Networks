#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t before_pid;
    before_pid = getpid();
    printf("Before exec. PID: %d\n", before_pid);
    execlp("ls", "ls", NULL);
    // Această linie nu va fi niciodată atinsa daca exec a reușit.
    printf("After exec. PID: %d\n", getpid());
    
    return 0;
}