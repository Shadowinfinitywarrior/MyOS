#include "libc.h"

int main(int argc, char **argv) {
    puts("MyOS Chat Client (stub)");
    if (argc > 1) printf("Connecting to: %s\n", argv[1]);
    return 0;
}
