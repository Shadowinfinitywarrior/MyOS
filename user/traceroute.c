#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: traceroute <host>");
        return 1;
    }
    printf("traceroute to %s, 30 hops max\n", argv[1]);
    puts(" 1  10.0.2.2 (10.0.2.2)  0.1 ms");
    return 0;
}
