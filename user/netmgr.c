#include "libc.h"
int main(void) {
    puts("[netmgr] Network configuration daemon started.");
    while (1) { sleep_ms(4000); }
    return 0;
}
