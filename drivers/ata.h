#ifndef ATA_H
#define ATA_H

#include "../include/types.h"

typedef struct ata_device ata_device_t;

void ata_init(void);
int ata_read_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, void *buf);

#endif
