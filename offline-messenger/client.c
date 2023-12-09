#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define NMAX 1024

extern int errno;

int port;
int sd;

void send_command();
void receive_command();

void intHandler(int get) // if the client is forced to be closed
{
    if(write(sd, "Quit", sizeof("Quit")) <= 0)
    {
        perror("[client] Error at write(1) to server.\n");
        exit(1);
    }
    close(sd);
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, intHandler);
    struct sockaddr_in server;
    pthread_t send_thread, receive_thread;

    if (argc != 3)
    {
        printf ("Syntax: %s <server_adress> <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[2]);
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Error at socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);
    
    if (connect(sd,(struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Error at connect().\n");
        return errno;
    }

    printf("Login\n");
    printf("Register\n");
    printf("Help\n");
    printf("Quit\n");   
    fflush(stdout);

    if(pthread_create(&send_thread, NULL, (void *)send_command, NULL) != 0)
    {
        printf("[client] Error at pthread-create(1).\n");
        return errno;
    }
    if(pthread_create(&receive_thread, NULL, (void *)receive_command, NULL) != 0)
    {
        printf("[client] Error at pthread-create(2).\n");
        return errno;
    }
    while (1) {}
    close (sd);
}

void send_command()
{
    char command[NMAX];
    bzero(command, NMAX);
    while(1)
    {
        read(0, command, sizeof(command));
        command[strlen(command) - 1] = '\0';
        if(!strncmp(command, "Quit", 4))
        {
            if(write(sd, command, sizeof(command)) <= 0)
            {
                perror("[client] Error at write(1) to server.\n");
                exit(1);
            }
            close(sd);
            exit(0);
        }
        else
        {
            if(write(sd, command, sizeof(command)) <= 0)
            {
                perror("Error writing to server.\n");
                exit(1);
            }
        }
        bzero(command, NMAX);
    }
}

void receive_command()
{
    char command_output[NMAX];
    bzero(command_output, NMAX);
    while(1)
    {
        if(read(sd, command_output, sizeof(command_output)) <= 0)
        {
            perror("[client] Eroare la read(1) de la server.\n");
            exit(1);
        }
        else
        {
            printf("%s", command_output); 
            fflush(stdout);
        }
        bzero(command_output, NMAX);
    }
}