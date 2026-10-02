#include "libc.h"

int main(void) {
    puts("ethtool eth0");
    puts("Settings for eth0:");
    puts("        Link detected: yes");
    puts("        Speed: 1000Mb/s");
    puts("        Duplex: Full");
    return 0;
}
