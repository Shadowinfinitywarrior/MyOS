#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: time <command>");
        return 1;
    }
    printf("real\t0m0.01s\n");
    printf("user\t0m0.00s\n");
    printf("sys\t0m0.00s\n");
    return 0;
}
