#include "libc.h"

int main(int argc, char **argv) {
    if (argc > 1) {
        printf("Setting hostname to %s\n", argv[1]);
        return 0;
    }
    puts("myos");
    return 0;
}
