#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "-a") == 0) {
        puts("MyOS 1.0 x86_64 MyOS MyOS 1.0");
    } else if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        puts("MyOS");
    } else if (argc > 1 && strcmp(argv[1], "-r") == 0) {
        puts("1.0");
    } else if (argc > 1 && strcmp(argv[1], "-m") == 0) {
        puts("x86_64");
    } else {
        puts("MyOS");
    }
    return 0;
}