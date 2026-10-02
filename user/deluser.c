#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: deluser <username>");
        return 1;
    }
    printf("Removing user %s (stub)\n", argv[1]);
    return 0;
}
