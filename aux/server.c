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
#include <time.h> 

#define read_channel "fifo1"
#define write_channel "fifo2"
#define fifo_child "child3_fifo"
#define path1 "/proc/"
#define path2 "/status"
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
    int is_logged = 0;
    while (1) 
    {
        if (access(write_channel, F_OK) == -1) 
        {
            continue; /// waiting for the client to connect to the server
        }
        int fd_write = open(write_channel, O_WRONLY);
        if (fd_write == -1) /// if the write channel is closed by the client, the server will keep trying to connect to the client again
        {
            continue;
        }
        printf("status: %d\n", is_logged);
        fflush(stdout);
        char client_response[NMAX] = {0};
        int client_response_fd;
        client_response_fd = read(fd_read, client_response, NMAX);
        if (client_response_fd == -1)
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
                close(pipefd[READ]);
                char name[64];
                strcpy(name, client_response + 8);
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
                        if (write(pipefd[WRITE], "You have sucessfully logged in!\n", 32) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                    else {
                        if (write(pipefd[WRITE], "You are already logged in!\n", 27) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                }
                else {
                    if (!is_logged)
                    {
                        if (write(pipefd[WRITE], "The name has not been found in the database.\n", 45) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                    else {
                        if (write(pipefd[WRITE], "You are already logged in, even though the name might not be found in database.\n", 80) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                }
                close(pipefd[WRITE]);
            }
            else { // parent
                close(pipefd[WRITE]);
                char response_child[NMAX] = {0};
                int response_child_fd = read(pipefd[READ], response_child, NMAX);
                if (response_child_fd == -1)
                {
                    perror("Error at reading from pipe");
                    exit(1);
                }
                if (write(fd_write, response_child, response_child_fd) == -1)
                {
                    perror("Error at writing in the channel");
                    exit(1);
                }
                wait(NULL);
                close(pipefd[READ]);
            }
        }
        else if (!strncmp(client_response, "logout", 6) && strlen(client_response) == 7) /// "logout"
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
        else if (!strncmp(client_response, "get-logged-users", 16) && strlen(client_response) == 17) // "get-logged-users"
        {
            if (is_logged)
            {
                int socket[2];
                if(socketpair(AF_UNIX, SOCK_STREAM, 0, socket) == -1)
                {
                    perror("Error at socketpair");
                    exit(1);
                }
                pid_t child2 = fork();
                if (child2 == 0) // child
                {
                    close(socket[READ]);
                    struct utmp *data;
                    time_t time;
                    setutent();
                    char * requested_data;
                    while ((data = getutent()) != NULL)
                    {
                        if (data -> ut_type == USER_PROCESS)
                        {
                            requested_data = (char*)malloc(NMAX);
                            time = data->ut_tv.tv_sec;
                            size_t requested_data_fd, requested_data_bytes;
                            requested_data_fd = snprintf(requested_data, NMAX, "%s %s %s", data->ut_user, data->ut_host, asctime(localtime(&time)));
                            if (requested_data_fd == -1)
                            {
                                perror("Error utmp");
                                exit(1);
                            }
                            if ((requested_data_bytes = write(socket[WRITE], requested_data, requested_data_fd)) == -1)
                            {
                                perror("Error at writing in socket");
                                exit(1);
                            }
                        }
                    }
                    endutent();
                    free(requested_data);
                    close(socket[WRITE]);
                    exit(11);
                }
                else { // parent
                    close(socket[WRITE]);
                    char get_requested_data[NMAX] = {0};
                    int stat;
                    waitpid(child2, &stat, 0);
                    if (WEXITSTATUS(stat) == 11)
                    {
                        int get_requested_data_fd = read(socket[READ], get_requested_data, NMAX);
                        if (get_requested_data_fd == -1)
                        {
                            perror("Error at reading from socket");
                            exit(1);
                        }
                        if (write(fd_write, get_requested_data, strlen(get_requested_data)) == -1)
                        {
                            perror("Error writing in the channel");
                            exit(1);
                        }
                    }
                    wait(NULL);
                    close(socket[READ]);
                }
            }
            else {
                if (write(fd_write, "You can't use this command if you are not logged in!\n", 53) == -1)
                {
                    perror("Error writing in the channel");
                    exit(1);
                }
            }
        }
        else if (!strncmp(client_response, "get-proc-info : ", 16))
        {
            if (is_logged)
            {
                if (access(fifo_child, F_OK) == -1) {
                    if (mkfifo(fifo_child, 0666) == -1) {
                        perror("Error creating FIFO child");
                        exit(1);
                    }
                }
                pid_t child3 = fork();
                if (child3 == -1)
                {
                    perror("Error at fork3");
                    exit(1);
                }
                if(child3 == 0) /// child
                {
                    int fd = open(fifo_child, O_WRONLY);
                    char get_pid[6] = {0};
                    strcpy(get_pid, client_response + 16);
                    get_pid[strlen(get_pid) - 1] = '\0';
                    char source_path[40] = {0}; /// proc/pid/status
                    strcat(source_path, path1);
                    strcat(source_path, get_pid);
                    strcat(source_path, path2);
                    FILE* file = fopen(source_path, "r");
                    if (file == NULL) /// if the file doesn't exist, we'll just write in the fifo a relevant message
                    {
                        if (write(fd, "The process doesn't exist\n", 26) == -1)
                        {
                            perror("Error writing in the child fifo");
                            exit(1);
                        }
                        fclose(file);
                        exit(0);
                    }
                    char data[NMAX] = {0};
                    char get_data[256];
                    /// name, state, ppid, uid, vmsize
                    int flag_name = 0, flag_state = 0, flag_ppid = 0, flag_uid = 0, flag_vmsize = 0;
                    while (fgets(get_data, sizeof(get_data), file)) /// get line by line and put the relevant ones in data
                    {
                        if (!strncmp(get_data, "Name:", 5))
                        {
                            flag_name = 1;
                            strcat(data, get_data);
                        }
                        if (!strncmp(get_data, "State:", 6))
                        {
                            flag_state = 1;
                            strcat(data, get_data);
                        }
                        if (!strncmp(get_data, "PPid:", 5))
                        {
                            flag_ppid = 1;
                            strcat(data, get_data);
                        }
                        if (!strncmp(get_data, "VmSize:", 7))
                        {
                            flag_vmsize = 1;
                            strcat(data, get_data);
                        }
                        if (!strncmp(get_data, "Uid:", 4))
                        {
                            flag_uid = 1;
                            strcat(data, get_data);
                        }
                    }
                    fclose(file);
                    /// in the case one of the field is missing, we will just add one which has the value "NULL" 
                    if (!flag_name)
                        strcat(data, "Name: NULL\n");
                    if (!flag_state)
                        strcat(data, "State: NULL\n");
                    if (!flag_ppid)
                        strcat(data, "PPid: NULL\n");
                    if (!flag_vmsize)
                        strcat(data, "VmSize: NULL\n");
                    if (!flag_uid)
                        strcat(data, "Uid : NULL\n");
                    if (write(fd, data, NMAX) == -1)
                    {
                        perror("Error writing in the child fifo");
                        exit(1);
                    }
                    close(fd);
                    exit(0);
                }
                else { /// parent
                    int fd = open(fifo_child, O_RDONLY);
                    char response_proc[NMAX] = {0};
                    int response_proc_fd;
                    response_proc_fd = read(fd, response_proc, NMAX);
                    if (response_proc_fd == -1)
                    {
                        perror("Error reading from the channel");
                        exit(1);
                    }
                    if (write(fd_write, response_proc, strlen(response_proc)) == -1)
                    {
                        perror("Error writing in the channel");
                        exit(1);
                    }
                    wait(NULL);
                    close(fd);
                    unlink(fifo_child);
                }
            }
            else {
                if (write(fd_write, "You can't use this command if you are not logged in!\n", 53) == -1)
                {
                    perror("Error writing in the channel");
                    exit(1);
                }
            }
        }
        else 
        {
            if (!strncmp(client_response, "quit", 4) && strlen(client_response) == 5) // we don't have to send a message to the client
            {
                is_logged = 0;
                close(fd_write);
                continue; /// this is so that will keep the server on even though the client stopped
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