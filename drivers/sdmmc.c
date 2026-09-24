#include "sdmmc.h"
#include <lib/printf.h>

static sdmmc_t dev;

int sdmmc_init(void) {
    dev.base_io = 0;
    dev.card_present = 0;
    kprintf("[sdmmc] init stub\n");
    return 0;
}

int sdmmc_read_blocks(uint32_t lba, void *buf, uint32_t count) {
    return -1;
}

int sdmmc_write_blocks(uint32_t lba, const void *buf, uint32_t count) {
    return -1;
}
