#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: logger <message>");
        return 1;
    }
    printf("Logging: ");
    for (int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    puts("");
    return 0;
}
