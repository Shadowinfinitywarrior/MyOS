#include "libc.h"

int main(int argc, char **argv) {
    const char *text = argc > 1 ? argv[1] : "MyOS";
    printf("%s\n", text);
    return 0;
}
