#ifndef VFS_H
#define VFS_H

#include "../include/types.h"

#define VFS_NAME_MAX 256

typedef struct vfs_node vfs_node_t;

typedef int (*read_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf);
typedef int (*write_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf);
typedef vfs_node_t *(*finddir_fn)(vfs_node_t *node, const char *name);
typedef vfs_node_t *(*readdir_fn)(vfs_node_t *node, uint32_t index);

struct vfs_node {
    char name[VFS_NAME_MAX];
    uint32_t flags;
    uint32_t length;
    uint32_t inode;
    void *private_data;
    vfs_node_t *parent;
    vfs_node_t *mount;
    read_fn read;
    write_fn write;
    finddir_fn finddir;
    readdir_fn readdir;
};

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02

void vfs_init(void);
void vfs_set_root(vfs_node_t *root);
vfs_node_t *vfs_resolve_path(const char *path);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf);
int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf);

#endif
