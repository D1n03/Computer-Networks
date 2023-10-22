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

    printf("Waiting for the writer to connect...\n");
    fd = open(FIFO_NAME, O_RDONLY);

    printf("Connected to the writer.\n");

    while (1) {
        if (read(fd, message, sizeof(message)) > 0) {
            printf("Received: %s\n", message);
            fflush(stdout);
        }
    }

    close(fd);
    return 0;
}
