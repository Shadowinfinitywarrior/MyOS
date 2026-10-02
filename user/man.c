#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("What manual page do you want?");
        return 1;
    }
    printf("No manual entry for %s\n", argv[1]);
    return 0;
}
