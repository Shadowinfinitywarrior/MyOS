#include "ide.h"
#include <lib/printf.h>

static ide_chan_t chans[2];

int ide_init(void) {
    kprintf("[ide] init stub\n");
    for (int i = 0; i < 2; i++) {
        chans[i].base_io = 0x1F0 + i*0x100;
        chans[i].channel = i;
        chans[i].present = 0;
    }
    return 0;
}

int ide_read_sectors(uint8_t channel, uint32_t lba, uint8_t *buf, uint32_t count) {
    // TODO: PIO/UDMA read
    return -1;
}

int ide_write_sectors(uint8_t channel, uint32_t lba, const uint8_t *buf, uint32_t count) {
    // TODO: PIO/UDMA write
    return -1;
}
