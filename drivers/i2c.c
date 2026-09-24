#include "i2c.h"
#include <lib/printf.h>

static i2c_bus_t bus;

int i2c_init(void) {
    bus.base_io = 0;
    bus.speed_hz = 100000;
    kprintf("[i2c] init stub\n");
    return 0;
}

int i2c_write(uint8_t dev_addr, uint8_t *buf, size_t len) {
    return -1;
}

int i2c_read(uint8_t dev_addr, uint8_t *buf, size_t len) {
    return -1;
}
