#include "libc.h"

int main(void) {
    puts("MyOS Network Configuration");
    puts("---------------------------");
    puts("Interface: eth0 (virtio-net)");
    puts("IP Address: 10.0.2.15/24 (DHCP)");
    puts("Gateway: 10.0.2.2");
    puts("DNS Server: 10.0.2.3");
    puts("MAC: 52:54:00:12:34:56");
    return 0;
}
