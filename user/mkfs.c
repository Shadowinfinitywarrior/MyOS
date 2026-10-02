#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: mkfs <device>");
        return 1;
    }
    printf("mkfs: formatting %s (stub)\n", argv[1]);
    return 0;
}
