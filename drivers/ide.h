#pragma once
#include <stdint.h>

typedef struct {
    uint16_t base_io;
    uint8_t channel;
    int present;
} ide_chan_t;

int ide_init(void);
int ide_read_sectors(uint8_t channel, uint32_t lba, uint8_t *buf, uint32_t count);
int ide_write_sectors(uint8_t channel, uint32_t lba, const uint8_t *buf, uint32_t count);
