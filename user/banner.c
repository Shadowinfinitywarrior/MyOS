#include "libc.h"

int main(int argc, char **argv) {
    const char *text = argc > 1 ? argv[1] : "MYOS";
    puts(text);
    return 0;
}
