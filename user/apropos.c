#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    printf("apropos: %s - no results\n", argv[1]);
    return 0;
}
