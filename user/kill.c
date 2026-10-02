#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: kill <pid>");
        return 1;
    }
    printf("Sending SIGTERM to process %s\n", argv[1]);
    return 0;
}
