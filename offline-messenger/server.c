#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>

#define PORT 2908
#define NMAX 256
#define db "database.txt"

extern int errno;

typedef struct thData{
    char username[64];
	int idThread;
	int cl;
}thData;

thData *clients[100];

char message_for_client[NMAX];
char message_from_client[NMAX];

static void *treat(void *);
void answer(void *);

int check_login_credentials(const char * email, const char * password, char * username)
{
    int found = 0;
    FILE * file = fopen(db, "r");
    if (file == NULL)
    {
        perror("[server] Error at fopen(1)");
        return errno;
    }
    else {
        char line[NMAX];
        while (fgets(line, sizeof(line), file) != NULL && !found)
        {
            char * get_username = strtok(line, " ");
            char * get_email = strtok(NULL, " ");
            char * get_password = strtok(NULL, "\n");
            if (!strcmp(email, get_email) && !strcmp(password, get_password))
            {
                strcpy(username, get_username);
                found = 1;
            }
        }
    }
    fclose(file);
    return found;
}

int register_account(const char* username, const char * email, const char * password)
{
    if(strchr(email, '@') == NULL)
        return -1; // email invalid
    int bytes;
    FILE *file = fopen(db, "a+");
    if(file == NULL)
    {
        perror("[server] Error at open(3).\n");
        return errno;
    }
    else 
    {
        char line[NMAX];
        while(fgets(line,sizeof(line),file) != NULL)
        {
            char *get_username = strtok(line, " ");
            char *get_email = strtok(NULL, " ");
            if(!strcmp(username, get_username))
                return 0; // the username is already used be another client
            if(!strcmp(email, get_email))
                return 1; // the email is already used be another client
        }
        char user_data[NMAX];
        bzero(user_data, NMAX);
        strcat(user_data, username);
        strcat(user_data, " ");
        strcat(user_data, email);
        strcat(user_data, " ");
        strcat(user_data, password);
        strcat(user_data, "\n");
        bytes = fprintf(file, "%s", user_data);
        if(bytes <= 0)
        {
            perror("[server] Error at fprintf(1).\n");
            return errno;
        }
    }
    fclose(file);
    return 2;
}

void add_client(thData *client)
{
    for (int i = 0; i < 100; i++)
        if (clients[i] == NULL)
        {
            clients[i] = client;
            break;
        }
}

void remove_client(thData *client)
{
    for(int i = 0; i < 100; i++)
        if(clients[i] == client)
        {
            clients[i] = NULL;
            break;
        }
}

int message_id(const char * fisier)
{
    int id = 0;
    FILE *file = fopen(fisier,"r");
    if(file == NULL)
    {
        perror("[server] Error at fopen(1).\n");
        return 0;
    }
    else 
    {
        char line[NMAX];
        while(fgets(line,sizeof(line), file) != NULL)
        {
            id += 1;
        }
    }
    fclose(file);
    return id;
}

int main ()
{
    struct sockaddr_in server;
    struct sockaddr_in from;	
    int sd; 
    pthread_t th[100];
	int id = 0;
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[server] Error at socket().\n");
        return errno;
    }
    int on = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
  
    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);
  
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server] Error at bind().\n");
        return errno;
    }

    if (listen (sd, 2) == -1)
    {
        perror ("[server] Error at listen().\n");
        return errno;
    }
    while (1)
    {
        int client;
        thData * td;     
        socklen_t length = sizeof(from);

        printf ("[server] Waiting at the port %d...\n",PORT);
        fflush (stdout);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
        {
            perror ("[server] Error at accept().\n");
            continue;
        }

        td=(struct thData*)malloc(sizeof(struct thData));	
        td->idThread=id++;
        td->cl=client;

	    pthread_create(&th[id], NULL, &treat, td);	      
				
	}  
};			

static void *treat(void * arg)
{		
	struct thData *tdL; 
	tdL= (struct thData*)arg;	
	printf ("[thread]- %d - Waiting the command...\n", tdL->idThread);
	fflush (stdout);		 
	pthread_detach(pthread_self());		
	answer((struct thData*)arg);
	close ((intptr_t)arg);
	return(NULL);		
};

void answer(void *arg)
{
	struct thData *tdL; 
	tdL = (struct thData*)arg;
    int bytes, is_logged = 0, view_msg_status = 0;
    while (1)
    {
        bzero(message_from_client, NMAX);
        if ((bytes = read(tdL->cl, message_from_client, sizeof(message_from_client))) <= 0)
        {
            printf("[Thread %d] ",tdL->idThread);
		    perror ("[Thread] Error at read(1) to the client.\n");
        }
        printf("The command received: %s\n", message_from_client);
        
        if (!strncmp(message_from_client, "Login", 5))
        {
            if (!is_logged) 
            {
                char email[NMAX], password[NMAX];
                bzero(email, NMAX);
                bzero(password, NMAX);
                strcpy(message_for_client, "Insert email: ");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(1) to the client.\n");
                }
                if ((bytes = read(tdL->cl, email, sizeof(email))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at read(2) from the client.\n");
                }
                strcpy(message_for_client, "Insert password: ");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(2) to the client.\n");
                }
                if ((bytes = read(tdL->cl, password, sizeof(password))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at read(3) from the client.\n");
                }
                char *username = malloc(64);
                int result = check_login_credentials(email, password, username);
                if (result)
                {
                    int connected = 0;
                    for (int i = 0; i < 100; i++)
                        if (clients[i] != NULL && !strcmp(clients[i]->username, username))
                            connected = 1;
                    if (connected)
                        strcpy(message_for_client, "You are already connected\n");
                    else 
                    {
                        add_client(tdL);
                        is_logged = 1;
                        strcpy(tdL->username, username);

                        char file[NMAX];
                        sprintf(file, "%s_received.txt", tdL->username);
                        int id = message_id(file);
                        char id_c[5];
                        sprintf(id_c, "%d", id);
                        strcpy(message_for_client, "You have connected successfully! ");
                        strcat(message_for_client, "You have ");
                        strcat(message_for_client, id_c);
                        strcat(message_for_client, " unread messages!\n");

                        printf("The user %s connected to the server.\n", tdL->username);
                        printf("The user %s is online.\n", tdL->username);
                    }
                }
                else
                {
                    strcpy(message_for_client, "Failed to login. Wrong email or password!\n");
                }
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(3) to the client.\n");
                }
                free(username);
            }
            else 
            {
                strcpy(message_for_client, "You are already connected with another account\n");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(4) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Register", 8))
        {
            if (!is_logged)
            {
                char username[NMAX], email[NMAX], password[NMAX];
                bzero(username, NMAX);
                bzero(email, NMAX);
                bzero(password, NMAX);
                strcpy(message_for_client, "Insert username: ");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(5) to the client.\n");
                }
                if ((bytes = read(tdL->cl, username, sizeof(username))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at read(4) from the client.\n");
                }
                strcpy(message_for_client, "Insert email: ");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(6) to the client.\n");
                }
                if ((bytes = read(tdL->cl, email, sizeof(email))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at read(5) from the client.\n");
                }
                strcpy(message_for_client, "Insert password: ");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(7) to the client.\n");
                }
                if ((bytes = read(tdL->cl, password, sizeof(password))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at read(6) from the client.\n");
                }  
                int ok = register_account(username, email, password); 
                switch (ok)
                {
                case -1: 
                    strcpy(message_for_client, "Email invalid\n");
                    break;

                case 0: 
                    strcpy(message_for_client, "The username is already being used by another account\n");
                    break;

                case 1: 
                    strcpy(message_for_client, "The email is already being used by another account\n");
                    break;

                case 2:  
                    add_client(tdL);
                    strcpy(tdL->username, username);
                    strcpy(message_for_client, "The account has been created succesfully!\n");

                    printf("The account with the username %s and email %s has been created.\n", tdL->username, email);
                    break;
                
                default:
                    break;
                }
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(7) to the client.\n");
                }
            }
            else 
            {
                strcpy(message_for_client, "Logout from the current accound if you want to create a new one.\n");
                if ((bytes = write(tdL->cl, message_for_client, sizeof(message_for_client))) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(8) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Logout", 6))
        {
            if (!is_logged)
                strcpy(message_for_client, "You are not connected!\nLogin\nRegister\nQuit\n");
            else 
            {
                printf("The user %s is offline!\n", tdL->username);

                if (view_msg_status)
                {
                    char file[NMAX];
                    sprintf(file, "%s_received.txt", tdL->username);
                    remove(file);
                }
                is_logged = 0;
                remove_client(tdL);
                strcpy(message_for_client, "You have successfully logout!\nLogin\nRegister\nQuit\n");
            }
            if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
            {
                printf("[Thread %d] ",tdL->idThread);
                perror ("[Thread] Error at write() to the client.\n");
            }
        }
        else if (!strncmp(message_from_client, "Quit", 4))
        {
            if (is_logged)
                printf("The user %s is offline.\n", tdL->username);
            else printf("The client has been closed\n");
            if (view_msg_status)
            {
                char file[NMAX];
                sprintf(file, "%s_received.txt", tdL->username);
                remove(file);
            }
            is_logged = 0;
            remove_client(tdL);
            break;
        }
        else 
        {
            strcpy(message_for_client, "This command doesn't exit!\n");
            if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
            {
                printf("[Thread %d] ",tdL->idThread);
                perror ("[Thread] Error at write() to the client.\n");
            }
            else printf ("[Thread %d] The message has been sent successfully.\n",tdL->idThread);	
        }
        bzero(message_for_client, NMAX);
    }
}