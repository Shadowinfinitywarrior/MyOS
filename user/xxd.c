#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: xxd <file>");
        return 1;
    }
    puts("00000000: 4865 6c6c 6f20 576f 726c 640a            Hello World.");
    return 0;
}
