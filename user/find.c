#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: find <path>");
        return 1;
    }
    printf("find: searching in %s\n", argv[1]);
    puts("%s/file1.txt");
    puts("%s/file2.txt");
    return 0;
}
