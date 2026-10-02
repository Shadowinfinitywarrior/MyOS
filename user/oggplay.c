#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: oggplay <file.ogg>");
        return 1;
    }
    printf("Playing OGG: %s\n", argv[1]);
    puts("OGG Vorbis player: not implemented (stub)");
    return 0;
}
