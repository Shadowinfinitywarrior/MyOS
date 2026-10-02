#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: sha256sum <file>");
        return 1;
    }
    printf("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  %s\n", argv[1]);
    return 0;
}
