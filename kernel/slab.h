#ifndef SLAB_H
#define SLAB_H

#include "../include/types.h"
#include "../include/system.h"
#include "../include/spinlock.h"

#define SLAB_MIN_ORDER 5   /* 32 bytes */
#define SLAB_MAX_ORDER 12  /* 4096 bytes */
#define SLAB_NUM_CACHES (SLAB_MAX_ORDER - SLAB_MIN_ORDER + 1)

void slab_init(void);
void *slab_alloc(uint32_t size);
void slab_free(void *ptr, uint32_t size);

#endif
