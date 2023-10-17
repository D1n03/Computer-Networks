#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("Eroare la fork");
        return 1;
    }
    // Parinte
    if (child_pid > 0) {
        printf("Procesul parinte. PID: %d, PPID: %d\n", getpid(), getppid());
    }
    // Copil
    else {
        printf("Procesul copil. PID: %d, PPID: %d\n", getpid(), getppid());
    }

    return 0;
}
