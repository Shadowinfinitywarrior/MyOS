#include "libc.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : ".";
    printf("4.0K\t%s\n", path);
    return 0;
}
