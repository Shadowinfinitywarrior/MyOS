#include "nvme.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static bool ready=false;
void nvme_init(uint32_t bar){ ready=true; kprintf("[NVMe] init at 0x%08X\n",bar); }
int nvme_read(uint64_t l,uint32_t c,void *b){ (void)l;(void)c;(void)b; return 0; }
int nvme_write(uint64_t l,uint32_t c,const void *b){ (void)l;(void)c;(void)b; return 0; }
uint64_t nvme_get_capacity(void){ return 1024*1024; }
