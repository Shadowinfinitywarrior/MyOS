#include "ramfs.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

typedef struct ramfs_node {
    vfs_node_t vfs;
    struct ramfs_node *children;
    struct ramfs_node *next;
} ramfs_node_t;

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name) {
    ramfs_node_t *rn = (ramfs_node_t *)node;
    for (ramfs_node_t *child = rn->children; child; child = child->next) {
        if (strcmp(child->vfs.name, name) == 0) {
            return &child->vfs;
        }
    }
    return NULL;
}

static void ramfs_add_child(vfs_node_t *parent, vfs_node_t *child) {
    ramfs_node_t *rn = (ramfs_node_t *)parent;
    ramfs_node_t *rc = (ramfs_node_t *)child;
    rc->next = rn->children;
    rn->children = rc;
}

vfs_node_t *ramfs_init(void) {
    ramfs_node_t *root = (ramfs_node_t *)kzalloc(sizeof(ramfs_node_t));
    strcpy(root->vfs.name, "/");
    root->vfs.flags = VFS_DIRECTORY;
    root->vfs.finddir = ramfs_finddir;
    root->children = NULL;
    root->next = NULL;
    return &root->vfs;
}

void ramfs_mount_dev(vfs_node_t *root, vfs_node_t *dev) {
    ramfs_add_child(root, dev);
}
