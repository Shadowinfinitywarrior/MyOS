#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: watch <command>");
        return 1;
    }
    puts("Every 2.0s: command");
    return 0;
}
