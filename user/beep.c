#include "libc.h"

int main(int argc, char **argv) {
    int freq = 440;
    int dur = 200;
    if (argc > 1) freq = atoi(argv[1]);
    if (argc > 2) dur = atoi(argv[2]);
    printf("Beeping at %dHz for %dms\n", freq, dur);
    // Stub - would use audio driver
    sleep_ms(dur);
    return 0;
}
