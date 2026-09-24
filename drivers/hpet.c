#include "hpet.h"
#include <lib/printf.h>

static hpet_t hpet;

int hpet_init(void) {
    // TODO: PCI probe for HPET, map MMIO, verify capability
    hpet.base_addr = 0;
    hpet.timer_count = 0;
    kprintf("[hpet] init stub\n");
    return 0;
}

uint64_t hpet_get_time_ns(void) {
    // TODO: read HPET main counter
    return 0;
}

void hpet_delay_us(uint32_t us) {
    // TODO: busy wait using HPET
}
