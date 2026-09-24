#ifndef NVME_H
#define NVME_H
#include "../include/types.h"
void nvme_init(uint32_t bar);
int nvme_read(uint64_t lba,uint32_t cnt,void *buf);
int nvme_write(uint64_t lba,uint32_t cnt,const void *buf);
uint64_t nvme_get_capacity(void);
#endif
