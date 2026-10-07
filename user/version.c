#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("MyOS 1.0.0");
    puts("Build: October 2026");
    puts("Architecture: x86_64");
    puts("Kernel: Monolithic with GUI");
    return 0;
}