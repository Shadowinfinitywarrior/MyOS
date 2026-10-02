#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: locate <name>");
        return 1;
    }
    printf("locate: %s\n", argv[1]);
    puts("/bin/%s");
    return 0;
}
