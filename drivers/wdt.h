#pragma once
#include <stdint.h>

typedef struct {
    uint16_t base_io;
    uint32_t timeout_ms;
} wdt_t;

int wdt_init(void);
void wdt_reset(void);
void wdt_set_timeout(uint32_t ms);
