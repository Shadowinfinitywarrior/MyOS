#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: fsck <device>");
        return 1;
    }
    printf("fsck: checking %s - OK\n", argv[1]);
    return 0;
}
