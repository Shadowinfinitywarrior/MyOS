#include "libc.h"

int main(int argc, char **argv) {
    const char *msg = argc > 1 ? argv[1] : "y";
    while (1) {
        puts(msg);
    }
    return 0;
}
