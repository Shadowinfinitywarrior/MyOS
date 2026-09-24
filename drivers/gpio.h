#pragma once
#include <stdint.h>

typedef struct {
    uint32_t base_addr;
    uint8_t bank;
} gpio_t;

int gpio_init(void);
void gpio_set(int pin, int val);
int gpio_get(int pin);
