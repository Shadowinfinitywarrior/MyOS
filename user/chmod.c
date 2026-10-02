#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: chmod <mode> <file>");
        return 1;
    }
    printf("chmod %s %s\n", argv[1], argv[2]);
    return 0;
}
