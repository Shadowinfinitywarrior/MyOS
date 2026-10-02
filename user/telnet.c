#include "libc.h"

int main(int argc, char **argv) {
    puts("telnet - stub implementation");
    if (argc >= 2) {
        printf("Trying %s...\n", argv[1]);
    }
    return 0;
}
