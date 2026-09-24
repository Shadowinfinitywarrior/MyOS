#pragma once
#include <stdint.h>

typedef struct {
    uint32_t base_io;
    int card_present;
} sdmmc_t;

int sdmmc_init(void);
int sdmmc_read_blocks(uint32_t lba, void *buf, uint32_t count);
int sdmmc_write_blocks(uint32_t lba, const void *buf, uint32_t count);
