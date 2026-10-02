#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: aplay <file.wav>");
        return 1;
    }
    printf("Playing audio file: %s\n", argv[1]);
    puts("aplay: WAV playback not implemented yet (stub)");
    return 0;
}
