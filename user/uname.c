#include "libc.h"

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "-a") == 0) {
        puts("MyOS 1.0.0 myos #1 Fri Oct 2 2026 x86_64 GNU/MyOS");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-r") == 0) {
        puts("1.0.0");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-m") == 0) {
        puts("x86_64");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        puts("MyOS");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        puts("myos");
        return 0;
    }
    puts("MyOS");
    return 0;
}
