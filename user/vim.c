#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: vim <file>");
        return 1;
    }
    printf("Vim 8.0 (stub) - editing %s\n", argv[1]);
    return 0;
}
