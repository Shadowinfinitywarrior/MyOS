#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: whereis <command>");
        return 1;
    }
    printf("%s: /bin/%s\n", argv[1], argv[1]);
    return 0;
}
