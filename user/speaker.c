#include "libc.h"

int main(int argc, char **argv) {
    int freq = 800;
    if (argc > 1) freq = atoi(argv[1]);
    printf("Speaker tone: %dHz\n", freq);
    sleep_ms(100);
    return 0;
}
