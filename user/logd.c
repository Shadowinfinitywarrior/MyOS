#include "libc.h"

int main(void) {
    puts("[logd] System logging daemon started.");
    while (1) {
        // In the future: read from a UNIX socket and write to /var/log/syslog
        sleep_ms(3000);
    }
    return 0;
}
