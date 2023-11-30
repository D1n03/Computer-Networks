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

void write_msg();
void read_msg();

void intHandler(int get) // if the client is forced to be closed
{
    int bytes;
    if((bytes = write(sd, "Quit", sizeof("Quit"))) <= 0)
    {
        perror("[client] Error at write(1) to server.\n");
    }
    close(sd);
    exit(0);
}

int main (int argc, char *argv[])
{
    signal(SIGINT, intHandler);
    struct sockaddr_in server;
    pthread_t send_thread, recv_thread;

    if (argc != 3)
    {
        printf ("Syntax: %s <server_adress> <port>\n", argv[0]);
        return -1;
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
    
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]Error at connect().\n");
        return errno;
    }

    printf("Login\n");
    printf("Register\n");
    printf("Quit\n");   
    fflush(stdout);

    if(pthread_create(&send_thread, NULL, (void *)write_msg, NULL)!=0)
    {
        printf("[client] Error at pthread-create(1).\n");
        return errno;
    }

    if(pthread_create(&recv_thread, NULL, (void *)read_msg, NULL)!=0)
    {
        printf("[client] Error at pthread-create(2).\n");
        return errno;
    }
    while (1) {}
    close (sd);
}

void write_msg()
{
    char msg[NMAX];
    int bytes;
    bzero(msg, NMAX);
    while(1)
    {
        read(0, msg, sizeof(msg));
        msg[strlen(msg) - 1] = '\0';
        if(strncmp(msg, "Quit", 4) == 0)
        {
            if((bytes = write(sd, msg, sizeof(msg))) <= 0)
            {
                perror("[client] Error at write(1) to server.\n");
            }
            close(sd);
            exit(0);
        }
        else
        {
            if((bytes = write(sd, msg, sizeof(msg))) <= 0)
            {
                perror("Error writing to server.\n");
            }
        }
        bzero(msg, NMAX);
    }
}

void read_msg()
{
    char msg[NMAX];
    int bytes;
    bzero(msg, NMAX);
    while(1)
    {
        if((bytes = read(sd, msg, sizeof(msg))) <= 0)
        {
            perror("[client] Eroare la read(1) de la server.\n");
        }
        else
        {
            printf("%s", msg); 
            fflush(stdout);
        }
        bzero(msg, NMAX);
    }
}