#include "libc.h"

int main(int argc, char **argv) {
    puts("Python 3.8 (stub) - MyOS minimal runtime");
    if (argc > 1) {
        printf("Running: %s\n", argv[1]);
    }
    puts(">>> print('Hello from MyOS Python!')");
    puts("Hello from MyOS Python!");
    return 0;
}
