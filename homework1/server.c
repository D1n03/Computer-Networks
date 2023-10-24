#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utmp.h>
#include <fcntl.h>

#define read_channel "fifo1"
#define write_channel "fifo2"
#define NMAX 1024
#define READ 0
#define WRITE 1 

int main() 
{
    if (access(read_channel, F_OK) == -1) 
    {
        if (mkfifo(read_channel, 0666) == -1)
        {
            perror("Failed to create fifo1");
            exit(1);
        }
    }
    int fd_read = open(read_channel, O_RDONLY);
    if (fd_read == -1) 
    {
        perror("Error at opening read channel");
        exit(1);
    }
    short is_logged = 0;
    while (1) 
    {
        if (access(write_channel, F_OK) == -1) 
        {
            if (mkfifo(write_channel, 0666) == -1)
            {
                perror("Failed to create fifo2");
                exit(1);
            }
        }
        int fd_write = open(write_channel, O_WRONLY);
        if (fd_write == -1) 
        {
            continue;
        }
        char client_response[NMAX] = {0};
        int client_response_size = 0;
        client_response_size = read(fd_read, client_response, NMAX);
        if (client_response_size == -1)
        {
            perror("Error reading from the channel");
            exit(1);
        }
        if (!strncmp(client_response, "login : ", 8)) /// "login : name"
        {
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                perror("Error at pipe");
                exit(1);
            }
            pid_t child1 = fork();
            if (child1 == -1)
            {
                perror("Error at fork");
                exit(1);
            }
            if (child1 == 0) // child
            {
                close(pipefd[0]);
                char name[64];
                strcpy(name, client_response);
                strcpy(name, name + 8);
                name[strlen(name) - 1] = '\0';
                int fd_database;
                fd_database = open("database.txt", O_RDONLY);
                if (fd_database == -1)
                {
                    perror("Error at opening the database");
                    exit(1);
                }
                char data_base_names[NMAX] = {0};
                int db_names_fd = read(fd_database, data_base_names, NMAX);
                if (db_names_fd == -1)
                {
                    perror("Error at reading from database");
                    exit(1);
                }    
                char *p = strtok(data_base_names, "\n");
                int found = 0;
                while (p && !found)
                {
                    if (!strcmp(name, p))
                        found = 1;
                    p = strtok(NULL, "\n");
                }
                if (found)
                {
                    if (!is_logged)
                    {
                        is_logged = 1;
                        if (write(pipefd[1], "You have sucessfully logged in!\n", 32) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                    else {
                        if (write(pipefd[1], "You are already logged in!\n", 27) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                }
                else {
                    if (!is_logged)
                    {
                        if (write(pipefd[1], "The name has not been found in the database.\n", 45) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                    else {
                        if (write(pipefd[1], "You are already logged in!\n", 27) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                }
                close(pipefd[1]);
            }
            else { // parent
                close(pipefd[1]);
                char response_child[NMAX] = {0};
                int response_child_size = read(pipefd[0], response_child, NMAX);
                if (response_child_size == -1)
                {
                    perror("Error at reading from pipe");
                    exit(1);
                }
                if (write(fd_write, response_child, response_child_size) == -1)
                {
                    perror("Error at writing in the channel");
                    exit(1);
                }
                wait(NULL);
                close(pipefd[0]);
            }
        }
        else if (!strncmp(client_response, "logout", 6) && strlen(client_response) == 7) /// logout
        {
            if (is_logged)
            {
                is_logged = 0;
                if (write(fd_write, "You have sucessfully logged out!\n", 33) == -1)
                {
                    perror("Error writing in the channel");
                    exit(1);
                }
            }
            else {
                if (write(fd_write, "You can't logged out if you are not logged in!\n", 47) == -1)
                {
                    perror("Error writing in the channel");
                    exit(1);
                }
            }
        }
        else if (!strncmp(client_response, "get-logged-users", 16))
        {
            char requested_info[NMAX] = {0};
            if (is_logged)
            {

            }
            else {

            }
        }
        else 
        {
            if (!strncmp(client_response, "quit", 4) && strlen(client_response) == 4)
            {
                close(fd_write);
                while (open(write_channel, O_WRONLY) == -1) // this loop will keep the server on even though the client stopped
                {
                }
            }
            else if (write(fd_write, "Unknown command\n", 16) == -1)
            {
                perror("Error writing in the channel");
                exit(1);
            }
        }
    }
    close(fd_read);
    unlink(read_channel);
    return 0;
}
