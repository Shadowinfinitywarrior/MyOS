#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: mp3play <file.mp3>");
        return 1;
    }
    printf("Playing MP3: %s\n", argv[1]);
    puts("mp3play: MP3 decoder not implemented yet (stub)");
    return 0;
}
