#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *p = strrchr(argv[1], '/');
    if (!p) { puts("."); return 0; }
    if (p == argv[1]) { puts("/"); return 0; }
    *p = '\0';
    printf("%s\n", argv[1]);
    return 0;
}
