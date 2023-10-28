#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utmp.h>
#include <fcntl.h>

#define write_channel "fifo1"
#define read_channel "fifo2"
#define NMAX 1024
#define READ 0
#define WRITE 1 


int main() 
{
    printf("Trying to connect to the server...\n");
    fflush(stdout); // a must, ensures that every message in the buffer is written to the console each time we are using printf()
    if (access(write_channel, F_OK) == -1) 
    {
        if (mkfifo(write_channel, 0666) == -1)
        {
            perror("Failed to create fifo1");
            exit(1);
        }
    }
    int fd_write = open(write_channel, O_WRONLY);
    if (fd_write == -1) 
    {
        perror("Error opening write channel");
        exit(1);
    }
    if (access(read_channel, F_OK) == -1) 
    {
        if (mkfifo(read_channel, 0666) == -1)
        {
            perror("Failed to create fifo2");
            exit(1);
        }
    }
    int fd_read = open(read_channel, O_RDONLY);
    if (fd_read == -1) 
    {
        perror("Error at opening read channel");
        exit(1);
    }
    /// diplay this message in the client to notify the user that the client connected to the server
    /// (the fifo are can be accesed by the client and the server)
    printf("Connected to the server!\n"); 
    fflush(stdout);
    while (1)
    {
        char buffer[NMAX] = {0};
        int buffer_fd = 0;
        printf("%s", "Command: ");
        fflush(stdout);
        buffer_fd = read(READ, buffer, NMAX);
        if (buffer_fd == -1)
        {
            perror("Error reading the input");
            exit(1);
        }
        buffer[strlen(buffer)] = '\0';
        if (write(fd_write, buffer, strlen(buffer)) == -1)
        {
            perror("Error writing in the channel");
            exit(1);
        }
        if (!strncmp(buffer, "quit", 4) && strlen(buffer) == 5) /// quit the client, but the server will still be on
        {
            close(fd_write);
            unlink(read_channel);
            exit(0);
        }
        // sleep(1);
        // get the server response, the exception is "quit" command which doesn't return a message to the client.
        char server_response[NMAX] = {0};
        int response_fd = 0;
        response_fd = read(fd_read, server_response, NMAX);
        if (response_fd == -1)
        {
            perror("Error reading from the channel");
            exit(1);
        }
        printf("Server response size: %ld\n", strlen(server_response));
        fflush(stdout);
        printf("%s", server_response);
        fflush(stdout);
    }
    close(fd_read);
    close(fd_write);
    unlink(read_channel);
    unlink(write_channel);
    return 0;
}