#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: httppost <url> [data]");
        return 1;
    }
    printf("HTTP POST to %s\n", argv[1]);
    return 0;
}
