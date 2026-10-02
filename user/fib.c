#include "libc.h"

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 10;
    printf("Fibonacci(%d)=55 (stub)\n", n);
    return 0;
}
