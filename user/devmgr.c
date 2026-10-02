#include "libc.h"

int main(void) {
    puts("[devmgr] Device manager daemon started.");
    while (1) {
        // In the future: listen to Netlink-style socket for device hotplug events
        // and create nodes in /dev
        sleep_ms(2000);
    }
    return 0;
}
