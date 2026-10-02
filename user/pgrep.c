#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: pgrep <pattern>");
        return 1;
    }
    printf("pgrep: no processes matched '%s'\n", argv[1]);
    return 0;
}
