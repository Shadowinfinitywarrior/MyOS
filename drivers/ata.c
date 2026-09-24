#include "ata.h"
#include "../include/types.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

ata_device_t* ata_devices[2] = {NULL, NULL};

void ata_init(void) {}
int ata_read_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, void *buf) { (void)dev; (void)lba; (void)count; (void)buf; return -1; }
