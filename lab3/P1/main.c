#include<stdio.h> 
#include<stdlib.h> 
#include<unistd.h> 
#include<sys/types.h> 
#include<string.h> 
#include<sys/wait.h> 

#define READ 0
#define WRITE 1 

int main() 
{ 
    int parent_to_child[2], child_to_parent[2];   
    pipe(parent_to_child);
    pipe(child_to_parent);
    pid_t pid = fork(); 
    if(pid > 0) 
    { // parent
        close(child_to_parent[WRITE]);
        close(parent_to_child[READ]);
        write(parent_to_child[WRITE], "I am the parent", 16);
        char buff[100];
        int len_read = read(child_to_parent[READ], buff, 100);
        buff[len_read] = '\0';
        printf("Read from child: %s\n", buff);
    } 
    else { // child
        close(parent_to_child[WRITE]);
        close(child_to_parent[READ]);
        write(child_to_parent[WRITE], "I am the child!", 16);
        char buff[100];
        int len_read = read(parent_to_child[READ], buff, 100);
        buff[len_read] = '\0';
        printf("Read from parent: %s\n", buff);
    }
    return 0;
} 
