#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: whois <domain>");
        return 1;
    }
    printf("Whois query for %s\n", argv[1]);
    puts("Domain: (stub)\nRegistrar: Example Registrar\nStatus: active");
    return 0;
}
