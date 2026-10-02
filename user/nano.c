#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: nano <file>");
        return 1;
    }
    printf("nano: opening %s (text editor stub)\n", argv[1]);
    return 0;
}
