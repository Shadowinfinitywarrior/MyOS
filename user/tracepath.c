#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: tracepath <host>");
        return 1;
    }
    printf("tracepath to %s, 30 hops max\n", argv[1]);
    puts(" 1:  192.168.1.1  1ms");
    puts(" 2:  (stub)        -");
    return 0;
}
