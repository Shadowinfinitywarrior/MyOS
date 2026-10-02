#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: stat <file>");
        return 1;
    }
    printf("  File: %s\n", argv[1]);
    puts("  Size: 4096       Blocks: 8          IO Block: 4096   directory");
    puts("Access: (0755/drwxr-xr-x)  Uid: 0  Gid: 0");
    return 0;
}
