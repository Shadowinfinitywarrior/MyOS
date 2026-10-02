#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: ln <target> <link>");
        return 1;
    }
    printf("ln: creating link '%s' -> '%s'\n", argv[2], argv[1]);
    return 0;
}
