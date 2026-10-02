#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: websearch <query>");
        return 1;
    }
    char query[256] = {0};
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(query, " ");
        strcat(query, argv[i]);
    }
    printf("Searching web for: %s\n", query);
    puts("Web search results (stub):");
    puts("1. MyOS Documentation - https://github.com/myos");
    puts("2. MyOS Project - Example result");
    return 0;
}
