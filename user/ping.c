#include "libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: ping <host>");
        return 1;
    }
    printf("PING %s (stub)\n", argv[1]);
    for (int i = 0; i < 4; i++) {
        printf("%d bytes from %s: icmp_seq=%d time=1ms\n", 64, argv[1], i+1);
        sleep_ms(1000);
    }
    printf("--- %s ping statistics ---\n", argv[1]);
    printf("4 packets transmitted, 4 received, 0%% packet loss\n");
    return 0;
}
