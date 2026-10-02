#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("info: No menu item specified");
        return 1;
    }
    printf("info: No info for %s\n", argv[1]);
    return 0;
}
