#include "virtio_blk.h"
#include "virtio.h"
#include "../kernel/heap.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static virtio_device_t *dev=NULL;
static virtqueue_t *vq=NULL;
static uint64_t cap=0;
void virtio_blk_init(void){
    dev=virtio_find_device(VIRTIO_DEV_BLK);
    if(!dev){ kprintf("[VIRTIO-BLK] no device\n"); return; }
    virtio_device_reset(dev);
    dev->mmio->status=1|2;
    uint64_t *cfg=(uint64_t*)((uint8_t*)dev->mmio+0x100);
    cap=*cfg;
    if(!cap) cap=1024*2048;
    virtio_setup_queue(dev,0,16);
    vq=dev->queues[0];
    virtio_device_ready(dev);
    kprintf("[VIRTIO-BLK] init cap=%llu\n",(unsigned long long)cap);
}
int virtio_blk_read(uint32_t sector,uint32_t cnt,void *buf){
    if(!dev || !vq) return -1;
    virtio_blk_req_t req;
    req.type=VIRTIO_BLK_T_IN;
    req.reserved=0;
    req.sector=sector;
    uint8_t status=0;
    vring_desc_t sg[3];
    sg[0].addr=(uint64_t)&req;
    sg[0].len=sizeof(req);
    sg[0].flags=0;
    sg[1].addr=(uint64_t)buf;
    sg[1].len=cnt*512;
    sg[1].flags=VRING_DESC_F_WRITE;
    sg[2].addr=(uint64_t)&status;
    sg[2].len=1;
    sg[2].flags=VRING_DESC_F_WRITE;
    int head=virtqueue_add_buf(vq,sg,1,2);
    if(head<0) return -1;
    virtqueue_kick(dev,0);
    while(virtqueue_get_buf(vq,NULL)<0){}
    if(status!=0) return -1;
    return cnt;
}
int virtio_blk_write(uint32_t sector,uint32_t cnt,const void *buf){
    if(!dev || !vq) return -1;
    virtio_blk_req_t req;
    req.type=VIRTIO_BLK_T_OUT;
    req.reserved=0;
    req.sector=sector;
    uint8_t status=0;
    vring_desc_t sg[3];
    sg[0].addr=(uint64_t)&req;
    sg[0].len=sizeof(req);
    sg[0].flags=0;
    sg[1].addr=(uint64_t)buf;
    sg[1].len=cnt*512;
    sg[1].flags=0;
    sg[2].addr=(uint64_t)&status;
    sg[2].len=1;
    sg[2].flags=VRING_DESC_F_WRITE;
    int head=virtqueue_add_buf(vq,sg,2,1);
    if(head<0) return -1;
    virtqueue_kick(dev,0);
    while(virtqueue_get_buf(vq,NULL)<0){}
    if(status!=0) return -1;
    return cnt;
}
uint64_t virtio_blk_get_capacity(void){ return cap; }
