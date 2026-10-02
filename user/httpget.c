#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: httpget <url>");
        return 1;
    }
    printf("HTTP GET: %s\n", argv[1]);
    return 0;
}
