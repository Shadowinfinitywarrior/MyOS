#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: mtr <host>");
        return 1;
    }
    puts("My traceroute  (stub)");
    return 0;
}
