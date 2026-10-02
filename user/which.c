#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: which <command>");
        return 1;
    }
    printf("/bin/%s\n", argv[1]);
    return 0;
}
