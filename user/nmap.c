#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: nmap <target>");
        return 1;
    }
    printf("Starting Nmap scan against %s (stub)\n", argv[1]);
    puts("Host is up (0.001s latency)");
    puts("Not shown: 65535 closed ports");
    return 0;
}
