#include "libc.h"

int main(void) {
    puts("eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>");
    puts("      inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255");
    puts("      ether 52:54:00:12:34:56  txqueuelen 1000");
    puts("      RX packets 0  bytes 0 (0.0 B)");
    puts("      TX packets 0  bytes 0 (0.0 B)");
    return 0;
}
