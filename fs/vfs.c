#include "vfs.h"
#include "../lib/string.h"

static vfs_node_t *vfs_root = NULL;

static vfs_dentry_t dcache_pool[VFS_DCACHE_SIZE];
static vfs_dentry_t dcache_head = { .next = &dcache_head, .prev = &dcache_head };
static int dcache_initialized = 0;

static void dcache_init(void) {
    for (int i = 0; i < VFS_DCACHE_SIZE; i++) {
        dcache_pool[i].next = &dcache_pool[(i + 1) % VFS_DCACHE_SIZE];
        dcache_pool[i].prev = &dcache_pool[(i + VFS_DCACHE_SIZE - 1) % VFS_DCACHE_SIZE];
    }
    dcache_initialized = 1;
}

void vfs_init(void) {
    vfs_root = NULL;
    if (!dcache_initialized) dcache_init();
}

void vfs_set_root(vfs_node_t *root) {
    vfs_root = root;
}

static vfs_dentry_t *dcache_lookup_internal(const char *path) {
    for (vfs_dentry_t *d = dcache_head.next; d != &dcache_head; d = d->next) {
        if (strcmp(d->name, path) == 0) {
            // Move to front (LRU)
            d->prev->next = d->next;
            d->next->prev = d->prev;
            d->next = dcache_head.next;
            d->prev = &dcache_head;
            dcache_head.next->prev = d;
            dcache_head.next = d;
            return d;
        }
    }
    return NULL;
}

static void dcache_add_internal(const char *path, vfs_node_t *node) {
    // Find a free slot or evict LRU
    vfs_dentry_t *d = dcache_head.prev;
    while (d != &dcache_head && d->node) {
        d = d->prev;
    }
    if (d == &dcache_head) {
        // Evict LRU
        d = dcache_head.prev;
        d->prev->next = d->next;
        d->next->prev = d->prev;
    }
    strncpy(d->name, path, VFS_NAME_MAX - 1);
    d->name[VFS_NAME_MAX - 1] = '\0';
    d->node = node;
    // Move to front
    if (d->next != &dcache_head || d->prev != &dcache_head) {
        if (d->next) d->next->prev = d->prev;
        if (d->prev) d->prev->next = d->next;
    }
    d->next = dcache_head.next;
    d->prev = &dcache_head;
    dcache_head.next->prev = d;
    dcache_head.next = d;
}

void vfs_dcache_add(const char *path, vfs_node_t *node) {
    dcache_add_internal(path, node);
}

vfs_node_t *vfs_dcache_lookup(const char *path) {
    vfs_dentry_t *d = dcache_lookup_internal(path);
    return d ? d->node : NULL;
}

vfs_node_t *vfs_resolve_path(const char *path) {
    if (!vfs_root) return NULL;
    if (!path || path[0] != '/') return NULL;

    vfs_node_t *cached = vfs_dcache_lookup(path);
    if (cached) return cached;

    vfs_node_t *cur = vfs_root;
    char tmp[512];
    strncpy(tmp, path + 1, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *tok = strtok(tmp, "/");
    while (tok) {
        if (!cur->finddir) return NULL;
        cur = cur->finddir(cur, tok);
        if (!cur) return NULL;
        tok = strtok(NULL, "/");
    }
    vfs_dcache_add(path, cur);
    return cur;
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name) {
    if (node && node->finddir) return node->finddir(node, name);
    return NULL;
}

int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
    if (node && node->read) return node->read(node, offset, size, buf);
    return -1;
}

int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const void *buf) {
    if (node && node->write) return node->write(node, offset, size, buf);
    return -1;
}