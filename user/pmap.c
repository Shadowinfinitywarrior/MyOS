#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: pmap <pid>");
        return 1;
    }
    printf("PID %s:\n", argv[1]);
    puts("0000000000400000      4K r-x-- shell");
    puts("0000000000401000      4K rw--- shell");
    puts("00007ffff7ff0000      8K rw--- [ anon ]");
    return 0;
}
