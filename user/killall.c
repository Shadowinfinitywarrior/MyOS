#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: killall <process>");
        return 1;
    }
    printf("Killing all processes named %s\n", argv[1]);
    return 0;
}
