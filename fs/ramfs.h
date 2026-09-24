#ifndef RAMFS_H
#define RAMFS_H
#include "../include/system.h"

#include "vfs.h"

vfs_node_t *ramfs_init(void);
void ramfs_mount_dev(vfs_node_t *root, vfs_node_t *dev);

#endif
