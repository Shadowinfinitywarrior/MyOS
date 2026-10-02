#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: aria2c <url>");
        return 1;
    }
    printf("aria2c: Downloading %s\n", argv[1]);
    return 0;
}
