#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: wavplay <file.wav>");
        return 1;
    }
    printf("Playing WAV: %s\n", argv[1]);
    puts("WAV player: stub implementation");
    return 0;
}
