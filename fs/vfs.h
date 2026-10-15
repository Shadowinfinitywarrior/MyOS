#ifndef VFS_H
#define VFS_H

#include "../include/types.h"
#include "../include/spinlock.h"

#define VFS_NAME_MAX 256
#define VFS_DCACHE_SIZE 256
#define MAX_MOUNTS 32
#define VFS_MAX_OPEN_FILES 256

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

#define MS_RDONLY   0x0001
#define MS_NOSUID   0x0002
#define MS_NODEV    0x0004

typedef struct vfs_node vfs_node_t;
typedef struct vfs_dentry vfs_dentry_t;

typedef int (*read_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf);
typedef int (*write_fn)(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf);
typedef vfs_node_t *(*finddir_fn)(vfs_node_t *node, const char *name);
typedef vfs_node_t *(*readdir_fn)(vfs_node_t *node, uint32_t index);

typedef struct file {
    vfs_node_t *node;
    uint64_t offset;
    uint32_t flags;
    uint32_t refcount;
    spinlock_t lock;
} file_t;

typedef struct {
    file_t *files[VFS_MAX_OPEN_FILES];
    spinlock_t lock;
} fd_table_t;

typedef struct mount {
    char mount_point[256];
    vfs_node_t *root;
    void *fs_info;
    uint32_t flags;
    struct mount *next;
} mount_t;

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
    int (*open)(vfs_node_t *node, uint32_t flags);
    int (*close)(vfs_node_t *node);
    int (*mkdir)(vfs_node_t *node, const char *name, uint32_t mode);
    int (*create)(vfs_node_t *node, const char *name, uint32_t mode);
    int (*unlink)(vfs_node_t *node, const char *name);
    int (*ioctl)(vfs_node_t *node, uint64_t request, void *arg);
    int (*poll)(vfs_node_t *node, int events);
};

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02

struct vfs_dentry {
    char name[VFS_NAME_MAX];
    vfs_node_t *node;
    struct vfs_dentry *next;
    struct vfs_dentry *prev;
};

void vfs_init(void);
void vfs_set_root(vfs_node_t *root);
vfs_node_t *vfs_resolve_path(const char *path);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf);
int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf);
void vfs_dcache_add(const char *path, vfs_node_t *node);
vfs_node_t *vfs_dcache_lookup(const char *path);

/* Extended Mount & File Operations */
int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags);
int vfs_umount(const char *target);
file_t *vfs_open(const char *path, uint32_t flags);
int vfs_close(file_t *file);

#endif