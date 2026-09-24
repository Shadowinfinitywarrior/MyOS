#pragma once
#include <stdint.h>

typedef struct {
    uint16_t base_io;
    uint32_t speed_hz;
    uint8_t mode;
} spi_bus_t;

int spi_init(void);
int spi_transfer(uint8_t *tx, uint8_t *rx, size_t len);
