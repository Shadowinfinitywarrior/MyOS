#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: host <hostname>");
        return 1;
    }
    printf("%s has address 192.168.1.100 (stub)\n", argv[1]);
    return 0;
}
