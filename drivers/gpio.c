#include "gpio.h"
#include <lib/printf.h>

static gpio_t gpio;

int gpio_init(void) {
    gpio.base_addr = 0;
    gpio.bank = 0;
    kprintf("[gpio] init stub\n");
    return 0;
}

void gpio_set(int pin, int val) {
    // TODO
}

int gpio_get(int pin) {
    return -1;
}
