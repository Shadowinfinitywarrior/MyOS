#include "module.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
void module_init(void){ kprintf("[MODULE] init\n"); }
int module_load(const char *n,const uint8_t *d,uint32_t s){ (void)n;(void)d;(void)s; return 0; }
