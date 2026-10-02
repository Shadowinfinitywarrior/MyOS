#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: rm <file>");
        return 1;
    }
    printf("rm: removing '%s'\n", argv[1]);
    return 0;
}
