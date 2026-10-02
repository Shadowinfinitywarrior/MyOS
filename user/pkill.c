#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: pkill <pattern>");
        return 1;
    }
    printf("pkill: no processes matched '%s'\n", argv[1]);
    return 0;
}
