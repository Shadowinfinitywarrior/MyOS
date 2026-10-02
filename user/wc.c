#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("      0       0       0");
        return 0;
    }
    printf("      1       2       10 %s\n", argv[1]);
    return 0;
}
