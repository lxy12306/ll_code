#include <unistd.h>
#include <stdio.h>

int main() {
    int i = 0;

    while (1) {
        printf("%d", ++i);
        usleep(100);
    }

    return 0;
}