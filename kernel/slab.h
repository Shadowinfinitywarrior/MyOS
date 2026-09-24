#ifndef SLAB_H
#define SLAB_H
#include "../include/system.h"
void slab_init(void);
void *slab_alloc(uint32_t size);
void slab_free(void *ptr,uint32_t size);
#endif
