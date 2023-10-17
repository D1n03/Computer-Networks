#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
    int myVar = 42;
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("Eroare la fork");
        return 1;
    }

    if (child_pid == 0) {
        myVar = 99;
        printf("Procesul copil. myVar = %d\n", myVar);
    }
    else {
        printf("Procesul parinte. myVar = %d\n", myVar);
    }
    printf("myVar = %d\n", myVar);
    return 0;
}
