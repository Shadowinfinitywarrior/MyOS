#include "virtio.h"
#include "../kernel/pmm.h"
#include "../kernel/heap.h"
#include "../kernel/paging.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#define MAX_VDEV 8
#define VIRTIO_MMIO_BASE 0xD0000000
#define VIRTIO_MMIO_SIZE 0x200
static virtio_device_t devs[MAX_VDEV];
static int ndev=0;
void virtio_device_reset(virtio_device_t *d){ d->mmio->status=0; }
void virtio_device_ready(virtio_device_t *d){ d->mmio->status|=VIRTIO_STATUS_OK; }
int virtio_setup_queue(virtio_device_t *d,int q,uint16_t sz){
    d->mmio->queue_sel=q;
    uint32_t max=d->mmio->queue_num_max; if(!max) return -1;
    if(sz>max) sz=max;
    uint32_t dsz=sz*sizeof(vring_desc_t);
    uint32_t asz=sizeof(vring_avail_t)+sz*sizeof(uint16_t);
    uint32_t usz=sizeof(vring_used_t)+sz*sizeof(vring_used_elem_t);
    (void)(((dsz+asz +4095)/4096)*4096 + ((usz+4095)/4096)*4096);
    uint32_t phys=pmm_alloc_page(); memset((void*)phys,0,4096);
    virtqueue_t *vq=(virtqueue_t*)kmalloc(sizeof(virtqueue_t));
    vq->num=sz; vq->desc=(vring_desc_t*)phys; vq->avail=(vring_avail_t*)(phys+dsz); vq->used=(vring_used_t*)(phys+dsz+asz);
    for(int i=0;i<sz;i++) vq->desc[i].next=(uint16_t)(i+1<sz?i+1:0xFFFF);
    vq->free_head=0; vq->num_free=sz; vq->last_used_idx=0;
    vq->free_list=(uint16_t*)kmalloc(sz*2);
    for(int i=0;i<sz;i++) vq->free_list[i]=i;
    d->mmio->queue_num=sz;
    d->mmio->queue_desc_low=phys;
    d->mmio->queue_driver_low=(uint32_t)vq->avail;
    d->mmio->queue_device_low=(uint32_t)vq->used;
    d->mmio->queue_ready=1;
    d->queues[q]=vq; d->num_queues=q+1;
    return 0;
}
int virtqueue_add_buf(virtqueue_t *vq,vring_desc_t *sg,uint16_t out,uint16_t in){
    uint16_t tot=out+in; if(vq->num_free<tot) return -1;
    uint16_t head=vq->free_head;
    uint16_t first=head;
    for(int i=0;i<tot;i++){
        vq->desc[head].addr=sg[i].addr;
        vq->desc[head].len=sg[i].len;
        vq->desc[head].flags=sg[i].flags | (i<tot-1?VRING_DESC_F_NEXT:0);
        vq->num_free--;
        if(i<tot-1){
            head=vq->desc[head].next;
        }else{
            vq->desc[head].next=0xFFFF;
        }
    }
    vq->free_head=head;
    uint16_t idx=vq->avail->idx%vq->num; vq->avail->ring[idx]=first; vq->avail->idx++; return first;
}
void virtqueue_kick(virtio_device_t *d,int q){ d->mmio->queue_notify=q; }
int virtqueue_get_buf(virtqueue_t *vq,uint32_t *len){
    if(vq->last_used_idx==vq->used->idx) return -1;
    uint16_t ui=vq->last_used_idx%vq->num; vring_used_elem_t *e=&vq->used->ring[ui];
    if(len) *len=e->len;
    vq->last_used_idx++;
    return e->id;
}
void virtio_init(void){
    for(int i=0;i<MAX_VDEV;i++){
        uint64_t pa = VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_SIZE;
        paging_map(pa, pa, PAGE_PRESENT | PAGE_WRITE);
        virtio_mmio_t *m=(virtio_mmio_t*)(uintptr_t)pa;
        if(m->magic!=0x74726976) continue;
        devs[ndev].mmio=m; devs[ndev].device_id=m->device_id; devs[ndev].irq=48+i;
        const char *n = m->device_id==1?"Net": m->device_id==2?"Blk":"?";
        kprintf("[VIRTIO] %s id=%u at 0x%08X\n",n,m->device_id,(uint32_t)pa); ndev++;
    }
    kprintf("[VIRTIO] %d devices\n",ndev);
}
virtio_device_t *virtio_find_device(uint32_t id){
    for(int i=0;i<ndev;i++) if(devs[i].device_id==id) return &devs[i];
    return NULL;
}
