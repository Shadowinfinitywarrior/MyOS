#include "spi.h"
#include <lib/printf.h>

static spi_bus_t bus;

int spi_init(void) {
    bus.base_io = 0;
    bus.speed_hz = 1000000;
    bus.mode = 0;
    kprintf("[spi] init stub\n");
    return 0;
}

int spi_transfer(uint8_t *tx, uint8_t *rx, size_t len) {
    return -1;
}
