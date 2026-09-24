#ifndef FAT16_H
#define FAT16_H

#include "vfs.h"
#include "../drivers/ata.h"

vfs_node_t *fat16_mount(ata_device_t *dev, uint32_t partition_lba);

#endif

