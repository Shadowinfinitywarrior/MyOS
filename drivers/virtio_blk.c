#include "virtio_blk.h"
#include "virtio.h"
#include "../kernel/heap.h"
#include "../kernel/paging.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static virtio_device_t *dev=NULL;
static virtqueue_t *vq=NULL;
static uint64_t cap=0;
static uint32_t size_max=512;

static uint64_t config_u64(virtio_device_t *d,uint16_t off){
    uint8_t b[8];
    virtio_read_config(d,off,b,8);
    uint64_t v=0;
    for(int i=7;i>=0;i--) v=(v<<8)|b[i];
    return v;
}

static uint32_t config_u32(virtio_device_t *d,uint16_t off){
    uint8_t b[4];
    virtio_read_config(d,off,b,4);
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}

void virtio_blk_init(void){
    dev=virtio_find_device(VIRTIO_DEV_BLK);
    if(!dev){ kprintf("[VIRTIO-BLK] no device\n"); return; }
    virtio_device_reset(dev);
    virtio_set_status(dev,VIRTIO_STATUS_ACK);
    virtio_set_status(dev,VIRTIO_STATUS_ACK|VIRTIO_STATUS_DRIVER);
    uint64_t host=virtio_get_features(dev);
    uint64_t want=host&(((uint64_t)1<<VIRTIO_F_VERSION_1)|(uint64_t)VIRTIO_BLK_F_SIZE_MAX|(uint64_t)VIRTIO_BLK_F_SEG_MAX);
    virtio_set_features(dev,want);
    uint16_t cb=(virtio_is_modern(dev)&&!dev->transitional)?8:0;
    cap=config_u64(dev,(uint16_t)(cb+0));
    if(want&VIRTIO_BLK_F_SIZE_MAX){
        uint32_t sm=config_u32(dev,(uint16_t)(cb+8));
        if(sm>=512&&sm<=4096) size_max=sm;
    }
    if(!cap) cap=1024*2048;
    if(virtio_setup_queue(dev,0,16)<0){
        kprintf("[VIRTIO-BLK] queue setup failed\n");
        virtio_device_reset(dev);
        dev=NULL;
        return;
    }
    vq=dev->queues[0];
    virtio_device_ready(dev);
    kprintf("[VIRTIO-BLK] ready cap=%llu sectors size_max=%u feats=0x%llx status=0x%x transport=%u\n",
            (unsigned long long)cap,size_max,(unsigned long long)want,
            (unsigned)virtio_get_status(dev),(unsigned)dev->transport);
}

static int submit(uint32_t sector,uint32_t cnt,void *buf,int write){
    if(!dev||!vq) return -1;
    if(!cap||(uint64_t)sector+cnt>cap) return -1;
    virtio_blk_req_t *req=kmalloc(sizeof(virtio_blk_req_t));
    if(!req) return -1;
    uint8_t *status=kmalloc(1);
    if(!status){ kfree(req); return -1; }
    req->type=write?VIRTIO_BLK_T_OUT:VIRTIO_BLK_T_IN;
    req->reserved=0;
    req->sector=sector;
    uint8_t *data_raw=kmalloc(cnt*512+size_max);
    if(!data_raw){ kfree(status); kfree(req); return -1; }
    uint8_t *data=(uint8_t*)(((uintptr_t)data_raw+size_max-1)&~((uintptr_t)size_max-1));
    if(write) memcpy(data,buf,(size_t)cnt*512);
    vring_desc_t sg[3];
    sg[0].addr=paging_get_physical((uint64_t)(uintptr_t)req);
    sg[0].len=sizeof(virtio_blk_req_t);
    sg[0].flags=0;
    sg[1].addr=paging_get_physical((uint64_t)(uintptr_t)data);
    sg[1].len=cnt*512;
    sg[1].flags=write?0:VRING_DESC_F_WRITE;
    sg[2].addr=paging_get_physical((uint64_t)(uintptr_t)status);
    sg[2].len=1;
    sg[2].flags=VRING_DESC_F_WRITE;
    *status=0xFF;
    int head=virtqueue_add_buf(vq,sg,write?2:1,write?1:2);
    if(head<0){ kfree(data_raw); kfree(status); kfree(req); return -1; }
    virtqueue_kick(dev,0);
    uint32_t spin=0;
    uint32_t len=0;
    int done=virtqueue_get_buf(vq,&len);
    while(done<0){
        if(++spin>50000000UL){
            virtqueue_release_buf(vq,(uint16_t)head);
            kfree(data_raw); kfree(status); kfree(req);
            return -1;
        }
        done=virtqueue_get_buf(vq,&len);
    }
    virtqueue_release_buf(vq,(uint16_t)done);
    uint8_t st=*status;
    int rc=(st==0)?0:-1;
    if(rc==0&&!write) memcpy(buf,data,(size_t)cnt*512);
    kfree(data_raw);
    kfree(status);
    kfree(req);
    return rc;
}

int virtio_blk_read(uint32_t sector,uint32_t cnt,void *buf){
    if(cnt==0) return 0;
    if((uint64_t)cnt*512>size_max) return -1;
    return submit(sector,cnt,buf,0);
}

int virtio_blk_write(uint32_t sector,uint32_t cnt,const void *buf){
    if(cnt==0) return 0;
    if((uint64_t)cnt*512>size_max) return -1;
    return submit(sector,cnt,(void*)(uintptr_t)buf,1);
}

uint64_t virtio_blk_get_capacity(void){ return cap; }
