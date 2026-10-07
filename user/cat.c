#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: cat <file>");
        return 1;
    }

    int fd = open(argv[1], 0);
    if (fd < 0) {
        printf("cat: cannot open '%s'\n", argv[1]);
        return 1;
    }

    char buf[512];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }
    close(fd);
    return 0;
}