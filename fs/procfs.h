#ifndef PROCFS_H
#define PROCFS_H
#include "../include/system.h"
typedef struct vfs_node vfs_node_t;
vfs_node_t *procfs_init(void);
#endif
