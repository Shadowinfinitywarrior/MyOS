#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static char *env_vars[] = {
    "PATH=/bin",
    "HOME=/home/user",
    "USER=user",
    "SHELL=/bin/sh",
    "TERM=vt100",
    NULL
};

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    for (int i = 0; env_vars[i]; i++) {
        puts(env_vars[i]);
    }
    return 0;
}