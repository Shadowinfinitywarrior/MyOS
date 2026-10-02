#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: open <file>");
        return 1;
    }
    printf("Opening %s\n", argv[1]);
    return 0;
}
