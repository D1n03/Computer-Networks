#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_MAX 4096

void listFilesRecursively(const char *basePath) {
    DIR *dir = opendir(basePath);
    if (!dir) {
        perror("Eroare la deschiderea directorului");
        return;
    }

    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        char filePath[PATH_MAX];
        struct stat statbuf;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;

        snprintf(filePath, sizeof(filePath), "%s/%s", basePath, dp->d_name);

        if (lstat(filePath, &statbuf) == -1) {
            perror("Eroare la obținerea informațiilor despre fișier");
            continue;
        }
        printf("%s\n", filePath);

        if (S_ISDIR(statbuf.st_mode)) {
            listFilesRecursively(filePath);
        }
    }
    closedir(dir);
}

int main() {
    DIR *dirp = opendir(".");
    if (dirp == NULL) {
        perror("Eroare la deschiderea directorului curent");
        exit(EXIT_FAILURE);
    }
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL) {
        printf("%s\n", dp->d_name);
    }
    closedir(dirp);
    listFilesRecursively(".");
    return 0;
}
