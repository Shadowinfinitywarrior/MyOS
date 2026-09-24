#include "vfs.h"
#include "../lib/string.h"

static vfs_node_t *vfs_root = NULL;

void vfs_init(void) { vfs_root = NULL; }
void vfs_set_root(vfs_node_t *root) { vfs_root = root; }

vfs_node_t *vfs_resolve_path(const char *path) {
    if (!vfs_root) return NULL;
    if (!path || path[0] != '/') return NULL;
    vfs_node_t *cur = vfs_root;
    char tmp[512];
    strncpy(tmp, path+1, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    char *tok = strtok(tmp, "/");
    while (tok) {
        if (!cur->finddir) return NULL;
        cur = cur->finddir(cur, tok);
        if (!cur) return NULL;
        tok = strtok(NULL, "/");
    }
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
