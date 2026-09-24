#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H
#include "../include/system.h"
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
typedef struct { uint32_t type; uint32_t reserved; uint64_t sector; } virtio_blk_req_t;
void virtio_blk_init(void);
int virtio_blk_read(uint32_t sector,uint32_t cnt,void *buf);
int virtio_blk_write(uint32_t sector,uint32_t cnt,const void *buf);
uint64_t virtio_blk_get_capacity(void);
#endif
