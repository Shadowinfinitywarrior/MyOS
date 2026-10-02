#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: mv <source> <dest>");
        return 1;
    }
    printf("mv: moving '%s' to '%s'\n", argv[1], argv[2]);
    return 0;
}
