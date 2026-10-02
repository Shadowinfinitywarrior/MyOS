#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: rmdir <directory>");
        return 1;
    }
    printf("rmdir: removing directory '%s'\n", argv[1]);
    return 0;
}
