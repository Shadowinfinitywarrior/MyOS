#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    int usec = atoi(argv[1]);
    sleep_ms(usec / 1000);
    return 0;
}
