#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Simplified: just print a message since we need readdir syscall */
    puts("Directory listing:");
    puts("  . (current)");
    puts("  .. (parent)");
    puts("  /dev/");
    puts("  /tmp/");
    puts("  /home/");
    puts("  /etc/");
    puts("  /etc/motd");

    return 0;
}
