#include "libc.h"

int main(int argc, char **argv) {
    puts("Node.js (stub) - MyOS JavaScript runtime");
    if (argc > 1) printf("Script: %s\n", argv[1]);
    puts("> console.log('Hello from Node on MyOS');");
    puts("Hello from Node on MyOS");
    return 0;
}
