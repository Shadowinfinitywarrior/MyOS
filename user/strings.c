#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: strings <file>");
        return 1;
    }
    puts("Hello World");
    puts("MyOS");
    return 0;
}
