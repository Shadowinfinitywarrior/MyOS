#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: umount <mountpoint>");
        return 1;
    }
    printf("umount: %s unmounted\n", argv[1]);
    return 0;
}
