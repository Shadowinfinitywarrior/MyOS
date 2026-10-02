#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int n = atoi(argv[1]);
    printf("%d is %sprime\n", n, n > 1 ? "a " : "not ");
    return 0;
}
