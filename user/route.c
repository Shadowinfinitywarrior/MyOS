#include "libc.h"

int main(void) {
    puts("Kernel IP routing table");
    puts("Destination     Gateway         Genmask         Iface");
    puts("0.0.0.0         10.0.2.2        0.0.0.0         eth0");
    puts("10.0.2.0        0.0.0.0         255.255.255.0   eth0");
    return 0;
}
