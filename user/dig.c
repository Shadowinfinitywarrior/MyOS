#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: dig <hostname>");
        return 1;
    }
    printf(";; QUESTION SECTION:\n;%s. IN A\n\n", argv[1]);
    printf(";; ANSWER SECTION:\n%s. 300 IN A 192.168.1.100 (stub)\n", argv[1]);
    return 0;
}
