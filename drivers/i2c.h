#pragma once
#include <stdint.h>

typedef struct {
    uint16_t base_io;
    uint32_t speed_hz;
} i2c_bus_t;

int i2c_init(void);
int i2c_write(uint8_t dev_addr, uint8_t *buf, size_t len);
int i2c_read(uint8_t dev_addr, uint8_t *buf, size_t len);
