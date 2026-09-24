#pragma once
#include <stdint.h>

// HPET - High Precision Event Timer
// PCI class 0x01 0x80
typedef struct {
    uint64_t base_addr;
    uint32_t timer_count;
} hpet_t;

int hpet_init(void);
uint64_t hpet_get_time_ns(void);
void hpet_delay_us(uint32_t us);
