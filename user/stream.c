#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: stream <url>");
        return 1;
    }
    printf("Streaming audio from: %s\n", argv[1]);
    puts("Audio streaming: stub implementation");
    return 0;
}
