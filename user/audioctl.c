#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: audioctl <command>");
        puts("Commands: play, stop, pause, volume, status, list");
        return 1;
    }
    if (strcmp(argv[1], "status") == 0) {
        puts("Audio Status: Ready");
        puts("Device: AC97/HDA (stub)");
        puts("Volume: 80%");
        puts("State: Stopped");
    } else if (strcmp(argv[1], "play") == 0) {
        puts("Playing audio...");
    } else if (strcmp(argv[1], "stop") == 0) {
        puts("Stopping audio...");
    } else if (strcmp(argv[1], "volume") == 0 && argc > 2) {
        printf("Setting volume to %s%%\n", argv[2]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
    }
    return 0;
}
