#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *p = strrchr(argv[1], '/');
    printf("%s\n", p ? p+1 : argv[1]);
    return 0;
}
