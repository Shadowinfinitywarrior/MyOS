#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: cp <source> <dest>");
        return 1;
    }
    printf("cp: copying '%s' to '%s'\n", argv[1], argv[2]);
    return 0;
}
