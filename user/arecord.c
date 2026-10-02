#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: arecord <file.wav>");
        return 1;
    }
    printf("Recording to: %s\n", argv[1]);
    puts("arecord: Audio recording not implemented yet (stub)");
    return 0;
}
