#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    int sec = atoi(argv[1]);
    sleep_ms(sec * 1000);
    return 0;
}
