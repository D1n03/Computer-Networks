#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

void sigusr1_handler(int signo)
{
   int fd = open("text1.txt", O_RDONLY);
   char buffer[100];
   ssize_t n = read(fd, buffer, sizeof(buffer));
   if (n > 0)
   {
      buffer[n] = '\0';
      printf("The parent process got SIGUSR1: %s\n", buffer);
   }
   close(fd);
}

void sigusr2_handler(int signo)
{
   int fd = open("text2.txt", O_RDONLY);
   char buffer[100];
   ssize_t n = read(fd, buffer, sizeof(buffer));
   if (n > 0)
   {
      buffer[n] = '\0';
      printf("The parent process got SIGUSR2: %s\n", buffer);
   }
   close(fd);
}

int main()
{
   pid_t child1, child2;
   child1 = fork();
   if (child1 == 0) {
      while (1) 
      {
         sleep(3);
         int fd = open("text1.txt", O_WRONLY | O_CREAT, 0644);
         const char* text = "This is the first child\n";
         write(fd, text, strlen(text));
         close(fd);
         kill(getppid(), SIGUSR1); // send signal to the parent
      }
   }
   else {
      child2 = fork();
      if (child2 == 0) {
         while (1) 
         {
            sleep(5);
            int fd = open("text2.txt", O_WRONLY | O_CREAT, 0644);
            const char* text = "This is the second child\n";
            write(fd, text, strlen(text));
            close(fd);
            kill(getppid(), SIGUSR2); // send signal to the parent
         }
      }
      else {
         signal(SIGUSR1, sigusr1_handler);
         signal(SIGUSR2, sigusr2_handler);
         wait(NULL);
         wait(NULL);
         return 0;
       }
    }
}	