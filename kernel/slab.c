#include "slab.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
void slab_init(void){ kprintf("[SLAB] init\n"); }
void *slab_alloc(uint32_t s){ (void)s; return NULL; }
void slab_free(void *p,uint32_t s){ (void)p;(void)s; }
