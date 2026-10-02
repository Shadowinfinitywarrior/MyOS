#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: nslookup <hostname>");
        return 1;
    }
    printf("Server:  10.0.2.3\n");
    printf("Address: 10.0.2.3#53\n\n");
    printf("Non-authoritative answer:\n");
    printf("Name:   %s\n", argv[1]);
    printf("Address: 192.168.1.100\n");
    return 0;
}
