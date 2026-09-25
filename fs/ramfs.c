#include "ramfs.h"
#include "vfs.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"

typedef struct ramfs_node {
    vfs_node_t vfs;
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
    struct ramfs_node *children;
    struct ramfs_node *next;
    struct ramfs_node *parent;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
} ramfs_node_t;

int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
    if (!node || !(node->flags & VFS_FILE)) return -1;
    ramfs_node_t *rn = (ramfs_node_t *)node;
    if (offset >= rn->size) return 0;
    uint32_t to_read = size;
    if (offset + to_read > rn->size) to_read = rn->size - offset;
    memcpy(buf, rn->data + offset, to_read);
    return to_read;
}

int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf) {
    if (!node || !(node->flags & VFS_FILE)) return -1;
    ramfs_node_t *rn = (ramfs_node_t *)node;
    if (offset + size > rn->capacity) {
        uint32_t new_cap = rn->capacity;
        while (new_cap < offset + size) new_cap = new_cap ? new_cap * 2 : 512;
        uint8_t *new_data = (uint8_t *)kmalloc(new_cap);
        if (!new_data) return -1;
        if (rn->data) {
            memcpy(new_data, rn->data, rn->size);
            kfree(rn->data);
        }
        rn->data = new_data;
        rn->capacity = new_cap;
    }
    memcpy(rn->data + offset, buf, size);
    if (offset + size > rn->size) rn->size = offset + size;
    return size;
}

vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name) {
    ramfs_node_t *rn = (ramfs_node_t *)node;
    for (ramfs_node_t *child = rn->children; child; child = child->next) {
        if (strcmp(child->vfs.name, name) == 0) {
            return &child->vfs;
        }
    }
    return NULL;
}

int ramfs_stat(vfs_node_t *node, void *stat_buf) {
    (void)node;
    (void)stat_buf;
    return 0;
}

static void ramfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    ramfs_node_t *rn = (ramfs_node_t *)parent;
    ramfs_node_t *rc = (ramfs_node_t *)child;
    rc->parent = (ramfs_node_t *)parent;
    rc->next = rn->children;
    rn->children = rc;
}

vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name) {
    ramfs_node_t *rn = (ramfs_node_t *)kzalloc(sizeof(ramfs_node_t));
    strcpy(rn->vfs.name, name);
    rn->vfs.flags = VFS_FILE;
    rn->vfs.read = ramfs_read;
    rn->vfs.write = ramfs_write;
    ramfs_add_child(parent, &rn->vfs);
    return &rn->vfs;
}

vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name) {
    ramfs_node_t *rn = (ramfs_node_t *)kzalloc(sizeof(ramfs_node_t));
    strcpy(rn->vfs.name, name);
    rn->vfs.flags = VFS_DIRECTORY;
    rn->vfs.finddir = ramfs_finddir;
    ramfs_add_child(parent, &rn->vfs);
    return &rn->vfs;
}

vfs_node_t *ramfs_init(void) {
    ramfs_node_t *root = (ramfs_node_t *)kzalloc(sizeof(ramfs_node_t));
    strcpy(root->vfs.name, "/");
    root->vfs.flags = VFS_DIRECTORY;
    root->vfs.finddir = ramfs_finddir;
    root->children = NULL;
    root->next = NULL;
    root->parent = NULL;
    return &root->vfs;
}

void ramfs_mount_dev(vfs_node_t *root, vfs_node_t *dev) {
    ramfs_add_child(root, dev);
}

int ramfs_create(const char *path, uint32_t mode) {
    (void)path;
    (void)mode;
    return -1;
}

vfs_node_t *ramfs_find(vfs_node_t *root, const char *path) {
    (void)root;
    return vfs_resolve_path(path);
}