#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: touch <file>");
        return 1;
    }

    int fd = open(argv[1], 0x200 | 0x400);  /* O_CREAT | O_WRONLY */
    if (fd < 0) {
        printf("touch: cannot create '%s'\n", argv[1]);
        return 1;
    }
    close(fd);
    return 0;
}