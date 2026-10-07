#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    bool newline = true;
    int i = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        i = 2;
    }

    for (; i < argc; i++) {
        write(1, argv[i], strlen(argv[i]));
        if (i < argc - 1) putchar(' ');
    }

    if (newline) putchar('\n');
    return 0;
}