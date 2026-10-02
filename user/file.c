#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: file <filename>");
        return 1;
    }
    printf("%s: ELF 64-bit LSB executable, x86-64, version 1 (SYSV)\n", argv[1]);
    return 0;
}
