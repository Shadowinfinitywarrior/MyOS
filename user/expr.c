#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 4) return 1;
    printf("%d\n", atoi(argv[1]) + atoi(argv[3]));
    return 0;
}
