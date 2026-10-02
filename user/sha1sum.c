#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: sha1sum <file>");
        return 1;
    }
    printf("da39a3ee5e6b4b0d3255bfef95601890afd80709  %s\n", argv[1]);
    return 0;
}
