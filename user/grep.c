#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: grep <pattern> <file>");
        return 1;
    }
    printf("grep '%s' in %s (stub)\n", argv[1], argv[2]);
    return 0;
}
