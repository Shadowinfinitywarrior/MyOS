#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: kill <pid> [signal]");
        return 1;
    }

    int pid = 0;
    for (int i = 0; argv[1][i]; i++) {
        pid = pid * 10 + (argv[1][i] - '0');
    }

    int sig = 9;  /* SIGKILL default */
    if (argc > 2) {
        sig = 0;
        for (int i = 0; argv[2][i]; i++) {
            sig = sig * 10 + (argv[2][i] - '0');
        }
    }

    int ret = kill(pid, sig);
    if (ret < 0) {
        printf("kill: failed to kill pid %d\n", pid);
        return 1;
    }
    return 0;
}