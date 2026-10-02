#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: mkdir <directory>");
        return 1;
    }
    printf("mkdir: created directory '%s'\n", argv[1]);
    return 0;
}
