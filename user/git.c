#include "libc.h"

int main(int argc, char **argv) {
    puts("Git (stub)");
    if (argc > 1) printf("git %s\n", argv[1]);
    puts("fatal: not a git repository (or any of the parent directories)");
    return 1;
}
