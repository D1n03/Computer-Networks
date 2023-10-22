#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FIFO_NAME "myfifo"

int main() {
    int fd;
    char message[1024];

    if (access(FIFO_NAME, F_OK) == -1) {
        if (mkfifo(FIFO_NAME, 0666) == -1) {
            perror("Error creating FIFO");
            exit(1);
        }
    }

    printf("Waiting for the reader to connect...\n");
    fd = open(FIFO_NAME, O_WRONLY);

    while (1) {
        printf("Enter a message: ");
        scanf(" %1023[^\n]", message);
        message[strlen(message)] = '\0';
        write(fd, message, strlen(message) + 1);
    }

    close(fd);
    return 0;
}
