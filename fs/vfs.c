#include "vfs.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "../lib/printf.h"

static vfs_node_t *vfs_root = NULL;
static mount_t *mount_head = NULL;
static spinlock_t vfs_lock = SPINLOCK_INIT;

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
    mount_head = NULL;
    if (!dcache_initialized) dcache_init();
}

void vfs_set_root(vfs_node_t *root) {
    vfs_root = root;
}

static vfs_dentry_t *dcache_lookup_internal(const char *path) {
    for (vfs_dentry_t *d = dcache_head.next; d != &dcache_head; d = d->next) {
        if (strcmp(d->name, path) == 0) {
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
    vfs_dentry_t *d = dcache_head.prev;
    while (d != &dcache_head && d->node) {
        d = d->prev;
    }
    if (d == &dcache_head) {
        d = dcache_head.prev;
        d->prev->next = d->next;
        d->next->prev = d->prev;
    }
    strncpy(d->name, path, VFS_NAME_MAX - 1);
    d->name[VFS_NAME_MAX - 1] = '\0';
    d->node = node;
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
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    dcache_add_internal(path, node);
    spin_unlock_irqrestore(&vfs_lock, flags);
}

vfs_node_t *vfs_dcache_lookup(const char *path) {
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    vfs_dentry_t *d = dcache_lookup_internal(path);
    vfs_node_t *node = d ? d->node : NULL;
    spin_unlock_irqrestore(&vfs_lock, flags);
    return node;
}

vfs_node_t *vfs_resolve_path(const char *path) {
    if (!vfs_root) return NULL;
    if (!path || path[0] != '/') return NULL;
    if (strcmp(path, "/") == 0) return vfs_root;

    /* Check mount table first */
    for (mount_t *m = mount_head; m; m = m->next) {
        if (strcmp(m->mount_point, path) == 0) {
            return m->root;
        }
    }

    vfs_node_t *cached = vfs_dcache_lookup(path);
    if (cached) return cached;

    vfs_node_t *cur = vfs_root;
    char tmp[512];
    strncpy(tmp, path + 1, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *tok = strtok(tmp, "/");
    while (tok) {
        if (cur->mount) cur = cur->mount;
        if (!cur->finddir) return NULL;
        cur = cur->finddir(cur, tok);
        if (!cur) return NULL;
        tok = strtok(NULL, "/");
    }
    if (cur->mount) cur = cur->mount;

    vfs_dcache_add(path, cur);
    return cur;
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name) {
    if (node && node->mount) node = node->mount;
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

int vfs_mount(const char *source, const char *target, const char *fstype, uint32_t flags) {
    (void)source;
    (void)fstype;
    vfs_node_t *target_node = vfs_resolve_path(target);
    if (!target_node) return -1;

    mount_t *m = (mount_t *)kmalloc(sizeof(mount_t));
    if (!m) return -1;
    strncpy(m->mount_point, target, sizeof(m->mount_point) - 1);
    m->mount_point[sizeof(m->mount_point) - 1] = '\0';
    m->root = target_node;
    m->flags = flags;
    m->next = mount_head;
    mount_head = m;

    kprintf("[VFS] Mounted %s on %s (%s)\n", source ? source : "none", target, fstype ? fstype : "generic");
    return 0;
}

int vfs_umount(const char *target) {
    mount_t **curr = &mount_head;
    while (*curr) {
        if (strcmp((*curr)->mount_point, target) == 0) {
            mount_t *to_free = *curr;
            *curr = to_free->next;
            kfree(to_free);
            kprintf("[VFS] Unmounted %s\n", target);
            return 0;
        }
        curr = &(*curr)->next;
    }
    return -1;
}

file_t *vfs_open(const char *path, uint32_t flags) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return NULL;
    if (node->open && node->open(node, flags) < 0) return NULL;

    file_t *f = (file_t *)kmalloc(sizeof(file_t));
    if (!f) return NULL;
    f->node = node;
    f->offset = 0;
    f->flags = flags;
    f->refcount = 1;
    spinlock_init(&f->lock);
    return f;
}

int vfs_close(file_t *file) {
    if (!file) return -1;
    uint64_t flags = spin_lock_irqsave(&file->lock);
    if (file->refcount > 0) file->refcount--;
    if (file->refcount == 0) {
        spin_unlock_irqrestore(&file->lock, flags);
        if (file->node && file->node->close) {
            file->node->close(file->node);
        }
        kfree(file);
        return 0;
    }
    spin_unlock_irqrestore(&file->lock, flags);
    return 0;
}