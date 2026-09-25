#ifndef RAMFS_H
#define RAMFS_H

#include "../include/types.h"
#include "vfs.h"

vfs_node_t *ramfs_init(void);
void ramfs_mount_dev(vfs_node_t *root, vfs_node_t *dev);
vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name);
vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name);
int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf);
int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf);
vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name);
int ramfs_stat(vfs_node_t *node, void *stat_buf);
int ramfs_create(const char *path, uint32_t mode);
vfs_node_t *ramfs_find(vfs_node_t *root, const char *path);

#endif