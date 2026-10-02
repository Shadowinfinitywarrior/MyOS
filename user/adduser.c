#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: adduser <username>");
        return 1;
    }
    printf("Adding user %s (stub)\n", argv[1]);
    return 0;
}
