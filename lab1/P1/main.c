#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

int main() {
    char c;
    while (1) {
        if (read(0, &c, 1) < 0) {
            perror("Eroare la citire");
            break;
        }
        if (c == '\n')
            continue;
        if (isalpha(c)) {
            write(1, &c, 1);
        } else {
            write(2, &c, 1);
        }
    }
    return 0;
}
