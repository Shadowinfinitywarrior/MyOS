#ifndef ATA_H
#define ATA_H

#include "../include/types.h"

struct ata_device {
    uint16_t io_base;      /* 0x1F0 or 0x170 */
    uint16_t ctrl_base;    /* 0x3F6 or 0x376 */
    uint8_t  drive;        /* 0 for master (0xA0), 1 for slave (0xB0) */
    bool     present;
    bool     is_atapi;
    bool     lba48_supported;
    uint32_t sectors;      /* total sectors */
    char     model[41];    /* Model string */
    char     serial[21];   /* Serial string */
};

typedef struct ata_device ata_device_t;

extern ata_device_t *ata_devices[4];

void ata_init(void);
int  ata_read_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, void *buf);
int  ata_write_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, const void *buf);
ata_device_t *ata_get_device(int idx);

#endif
