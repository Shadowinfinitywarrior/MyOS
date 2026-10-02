#include "libc.h"

int main(int argc, char **argv) {
    puts("SSH Client (stub)");
    if (argc >= 2) printf("Connecting to: %s\n", argv[1]);
    return 0;
}
