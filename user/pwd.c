#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static char cwd[256] = "/";

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    puts(cwd);
    return 0;
}