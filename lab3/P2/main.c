#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1
#define MAX 1024

int main()
{
    while (1)
    {
        int parent_to_child[2], child_to_parent[2];
        if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1)
        {
            perror("Error creating pipes");
            exit(1);
        }

        pid_t pid = fork();
        if (pid > 0)
        { // PARENT
            close(child_to_parent[WRITE]);
            close(parent_to_child[READ]);

            char input[MAX];
            printf("Enter a message: ");
            scanf(" %[^\n]", input); // Citim pana la Enter

            write(parent_to_child[WRITE], input, strlen(input) + 1);
            close(parent_to_child[WRITE]);

            char output[MAX];
            read(child_to_parent[READ], output, sizeof(output));
            printf("Parent received: %s\n", output);

            close(child_to_parent[READ]);
            wait(NULL);
        }
        else
        { // CHILD
            close(child_to_parent[READ]);
            close(parent_to_child[WRITE]);

            char input[MAX];
            read(parent_to_child[READ], input, sizeof(input));
            close(parent_to_child[READ]);

            int i;
            for (i = 0; input[i]; i++)
            {
                if (input[i] >= 'a' && input[i] <= 'z')
                    input[i] -= 32;
                else if (input[i] >= 'A' && input[i] <= 'Z')
                    input[i] += 32;
            }

            write(child_to_parent[WRITE], input, strlen(input) + 1);
            close(child_to_parent[WRITE]);
            exit(0);
        }
    }
    return 0;
}
