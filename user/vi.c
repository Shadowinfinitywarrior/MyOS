#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: vi <file>");
        return 1;
    }
    printf("vi: %s\n", argv[1]);
    return 0;
}
