#include "wdt.h"
#include <lib/printf.h>

static wdt_t wdt;

int wdt_init(void) {
    wdt.base_io = 0;
    wdt.timeout_ms = 5000;
    kprintf("[wdt] init stub\n");
    return 0;
}

void wdt_reset(void) {
    // TODO: kick watchdog
}

void wdt_set_timeout(uint32_t ms) {
    wdt.timeout_ms = ms;
}
