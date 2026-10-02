#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: lynx <url>");
        return 1;
    }
    puts("Lynx: Text-based web browser (stub)");
    printf("URL: %s\n", argv[1]);
    return 0;
}
