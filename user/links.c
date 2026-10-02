#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: links <url>");
        return 1;
    }
    puts("Links text browser - stub mode");
    return 0;
}
