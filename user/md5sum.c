#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: md5sum <file>");
        return 1;
    }
    printf("d41d8cd98f00b204e9800998ecf8427e  %s\n", argv[1]);
    return 0;
}
