#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("Filesystem     1K-blocks    Used Available Use% Mounted on");
    puts("ramfs            524288       0    524288   0% /");
    puts("devfs                 0       0         0   -  /dev");
    return 0;
}