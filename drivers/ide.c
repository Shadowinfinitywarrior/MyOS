#include "ide.h"
#include "ata.h"
#include "../lib/printf.h"

static ide_chan_t chans[2];

int ide_init(void) {
    ata_init();
    for (int i = 0; i < 2; i++) {
        chans[i].base_io = 0x1F0 + i * 0x100;
        chans[i].channel = (uint8_t)i;
        chans[i].present = (ata_get_device(i * 2) != NULL) ? 1 : 0;
    }
    kprintf("[IDE] Initialized 2 IDE channels (%s, %s)\n",
            chans[0].present ? "Primary active" : "Primary empty",
            chans[1].present ? "Secondary active" : "Secondary empty");
    return 0;
}

int ide_read_sectors(uint8_t channel, uint32_t lba, uint8_t *buf, uint32_t count) {
    if (channel > 1) return -1;
    ata_device_t *dev = ata_get_device(channel * 2);
    if (!dev) return -1;
    return ata_read_sectors(dev, lba, count, buf);
}

int ide_write_sectors(uint8_t channel, uint32_t lba, const uint8_t *buf, uint32_t count) {
    if (channel > 1) return -1;
    ata_device_t *dev = ata_get_device(channel * 2);
    if (!dev) return -1;
    return ata_write_sectors(dev, lba, count, buf);
}
