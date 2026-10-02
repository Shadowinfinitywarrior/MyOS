#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: axel <url>");
        return 1;
    }
    printf("axel: Downloading %s (multi-threaded stub)\n", argv[1]);
    return 0;
}
