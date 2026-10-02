#include "libc.h"

int main(int argc, char **argv) {
    puts("GCC (cross-compiler stub in userspace)");
    if (argc > 1) printf("Compiling: %s\n", argv[1]);
    return 0;
}
