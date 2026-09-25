#include "virtio.h"
#include "pci.h"
#include "../include/system.h"
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
#define VRING_AVAIL_F_NO_INTERRUPT 1
#define MMIO_CONFIG_BASE 0x100
#define PCI_CAP_ID_VNDR 0x09
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG 3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VQ_NO_DESC 0xFFFF
static virtio_device_t devs[MAX_VDEV];
static int ndev=0;

int virtio_is_modern(virtio_device_t *d){ return d->transport==VIRTIO_TRANSPORT_PCI; }

uint64_t virtio_get_features(virtio_device_t *d){
    if(virtio_is_modern(d)){
        uint32_t f0,f1;
        d->common->device_feature_select=0; f0=d->common->device_feature;
        d->common->device_feature_select=1; f1=d->common->device_feature;
        d->common->device_feature_select=0;
        return (uint64_t)f0|((uint64_t)f1<<32);
    }
    d->mmio->device_features_sel=0;
    return d->mmio->device_features;
}

void virtio_set_features(virtio_device_t *d,uint64_t f){
    if(virtio_is_modern(d)){
        d->common->driver_feature_select=0; d->common->driver_feature=(uint32_t)(f&0xFFFFFFFFu);
        d->common->driver_feature_select=1; d->common->driver_feature=(uint32_t)(f>>32);
        d->common->driver_feature_select=0;
        return;
    }
    d->mmio->driver_features_sel=0;
    d->mmio->driver_features=(uint32_t)f;
}

uint32_t virtio_get_status(virtio_device_t *d){
    if(virtio_is_modern(d)) return d->common->device_status;
    return d->mmio->status;
}

void virtio_set_status(virtio_device_t *d,uint32_t s){
    if(virtio_is_modern(d)){ d->common->device_status=(uint8_t)s; return; }
    d->mmio->status=s;
}

void virtio_device_reset(virtio_device_t *d){
    virtio_set_status(d,0);
    for(volatile int i=0;i<1000;i++){}
}

void virtio_device_ready(virtio_device_t *d){
    virtio_set_status(d,virtio_get_status(d)|VIRTIO_STATUS_OK);
}

void virtio_read_config(virtio_device_t *d,uint16_t off,void *dst,uint32_t len){
    uint8_t *o=(uint8_t*)dst;
    if(virtio_is_modern(d)){
        for(uint32_t i=0;i<len;i++) o[i]=d->devcfg[off+i];
        return;
    }
    for(uint32_t i=0;i<len;i++) o[i]=((volatile uint8_t*)(uintptr_t)d->mmio)[MMIO_CONFIG_BASE+off+i];
}

int virtio_setup_queue(virtio_device_t *d,int q,uint16_t sz){
    if(q<0||q>=8) return -1;
    uint32_t max;
    if(virtio_is_modern(d)){
        d->common->queue_select=(uint16_t)q;
        max=d->common->queue_size;
    }else{
        d->mmio->queue_sel=(uint32_t)q;
        max=d->mmio->queue_num_max;
    }
    if(!max) return -1;
    if(sz>max) sz=(uint16_t)max;
    uint32_t dsz=(uint32_t)sz*sizeof(vring_desc_t);
    uint32_t asz=4u+(uint32_t)sz*2u;
    asz=(asz+3u)&~3u;
    uint32_t usz=4u+(uint32_t)sz*sizeof(vring_used_elem_t);
    uint32_t total=dsz+asz+usz;
    uint32_t pages=(total+4095u)/4096u;
    uint64_t phys=pmm_alloc_contiguous(pages);
    if(!phys){ kprintf("[VIRTIO] queue %d alloc failed (%u pages)\n",q,pages); return -1; }
    memset((void*)(uintptr_t)phys,0,(size_t)pages*4096u);
    virtqueue_t *vq=(virtqueue_t*)kmalloc(sizeof(virtqueue_t));
    if(!vq){ pmm_free_page(phys); return -1; }
    vq->num=sz;
    vq->desc=(vring_desc_t*)(uintptr_t)phys;
    vq->avail=(vring_avail_t*)(uintptr_t)(phys+dsz);
    vq->used=(vring_used_t*)(uintptr_t)(phys+dsz+asz);
    vq->avail->flags=VRING_AVAIL_F_NO_INTERRUPT;
    vq->used->flags=0;
    for(uint16_t i=0;i<sz;i++) vq->desc[i].next=(i+1<sz)?(uint16_t)(i+1):VQ_NO_DESC;
    vq->free_head=0; vq->num_free=sz; vq->last_used_idx=0;
    if(virtio_is_modern(d)){
        d->common->queue_select=(uint16_t)q;
        d->common->queue_size=sz;
        d->common->queue_desc=phys;
        d->common->queue_driver=phys+dsz;
        d->common->queue_device=phys+dsz+asz;
        d->common->queue_enable=1;
    }else{
        d->mmio->queue_num=sz;
        d->mmio->queue_desc_low=(uint32_t)phys;
        d->mmio->queue_desc_high=(uint32_t)(phys>>32);
        d->mmio->queue_driver_low=(uint32_t)(phys+dsz);
        d->mmio->queue_driver_high=(uint32_t)((phys+dsz)>>32);
        d->mmio->queue_device_low=(uint32_t)(phys+dsz+asz);
        d->mmio->queue_device_high=(uint32_t)((phys+dsz+asz)>>32);
        d->mmio->queue_ready=1;
    }
    d->queues[q]=vq;
    if(q+1>d->num_queues) d->num_queues=q+1;
    return 0;
}

static uint16_t vq_alloc_desc(virtqueue_t *vq){
    if(!vq||vq->num_free==0) return VQ_NO_DESC;
    uint16_t head=vq->free_head;
    vq->free_head=vq->desc[head].next;
    vq->num_free--;
    return head;
}

static void vq_free_desc(virtqueue_t *vq,uint16_t idx){
    vq->desc[idx].next=vq->free_head;
    vq->free_head=idx;
    vq->num_free++;
}

int virtqueue_add_buf(virtqueue_t *vq,vring_desc_t *sg,uint16_t out,uint16_t in){
    uint16_t tot=(uint16_t)(out+in);
    if(!vq||tot==0||tot>8) return -1;
    uint16_t ids[8];
    for(uint16_t i=0;i<tot;i++){
        ids[i]=vq_alloc_desc(vq);
        if(ids[i]==VQ_NO_DESC){
            for(uint16_t j=0;j<i;j++) vq_free_desc(vq,ids[j]);
            return -1;
        }
    }
    for(uint16_t i=0;i<tot;i++){
        vq->desc[ids[i]].addr=sg[i].addr;
        vq->desc[ids[i]].len=sg[i].len;
        vq->desc[ids[i]].flags=sg[i].flags|((i+1<tot)?VRING_DESC_F_NEXT:0);
        vq->desc[ids[i]].next=(i+1<tot)?ids[i+1]:VQ_NO_DESC;
    }
    uint16_t first=ids[0];
    vq->avail->ring[(uint16_t)(vq->avail->idx%vq->num)]=first;
    vq->avail->idx++;
    __asm__ __volatile__("" ::: "memory");
    return (int)first;
}

void virtqueue_kick(virtio_device_t *d,int q){
    if(virtio_is_modern(d)){
        d->common->queue_select=(uint16_t)q;
        uint32_t off=d->common->queue_notify_off;
        volatile uint16_t *np=(volatile uint16_t*)(uintptr_t)((uintptr_t)d->notify+off*d->notify_mult);
        *np=(uint16_t)q;
        return;
    }
    d->mmio->queue_notify=(uint32_t)q;
}

int virtqueue_get_buf(virtqueue_t *vq,uint32_t *len){
    if(!vq) return -1;
    uint16_t used_idx=(uint16_t)(vq->used->idx&0xFFFF);
    if(vq->last_used_idx==used_idx) return -1;
    uint16_t slot=(uint16_t)(vq->last_used_idx%vq->num);
    vring_used_elem_t *e=&vq->used->ring[slot];
    if(len) *len=e->len;
    vq->last_used_idx++;
    return (int)e->id;
}

void virtqueue_release_buf(virtqueue_t *vq,uint16_t head){
    if(!vq||head==VQ_NO_DESC) return;
    uint16_t cur=head;
    for(uint16_t guard=0;guard<64;guard++){
        uint16_t next=vq->desc[cur].next;
        vq->desc[cur].addr=0;
        vq->desc[cur].len=0;
        vq->desc[cur].flags=0;
        vq->desc[cur].next=VQ_NO_DESC;
        vq_free_desc(vq,cur);
        if(next==VQ_NO_DESC) break;
        cur=next;
    }
}

static int find_virtio_cap(uint8_t bus,uint8_t slot,uint8_t func,uint8_t want,virtio_pci_cap_t *out){
    uint8_t off=(uint8_t)(pci_read_cfg8(bus,slot,func,0x34)&0xFC);
    for(int guard=0;guard<48&&off>=0x40&&off<0xFC;guard++){
        uint8_t id=pci_read_cfg8(bus,slot,func,off);
        uint8_t next=pci_read_cfg8(bus,slot,func,(uint8_t)(off+1));
        if(id==PCI_CAP_ID_VNDR){
            virtio_pci_cap_t c;
            c.cap_vndr=id;
            c.cap_next=next;
            c.cap_len=pci_read_cfg8(bus,slot,func,(uint8_t)(off+2));
            c.cfg_type=pci_read_cfg8(bus,slot,func,(uint8_t)(off+3));
            c.bar=pci_read_cfg8(bus,slot,func,(uint8_t)(off+4));
            c.offset=pci_read_cfg(bus,slot,func,(uint8_t)(off+8));
            c.length=pci_read_cfg(bus,slot,func,(uint8_t)(off+12));
            if(c.cfg_type==want){ *out=c; return (int)off; }
        }
        if(!next||next==off) break;
        off=(uint8_t)(next&0xFC);
    }
    return -1;
}

static uint64_t pci_bar_addr(uint8_t bus,uint8_t slot,uint8_t func,uint8_t bar){
    uint32_t lo=pci_read_cfg(bus,slot,func,(uint8_t)(0x10+bar*4));
    if(!(lo&1)){
        uint64_t addr=lo&0xFFFFFFF0u;
        if(lo&0x4) addr|=(uint64_t)pci_read_cfg(bus,slot,func,(uint8_t)(0x14+bar*4))<<32;
        return addr;
    }
    return lo&0xFFFFFFFCu;
}

static uint64_t pci_bar_size(uint8_t bus,uint8_t slot,uint8_t func,uint8_t bar){
    uint32_t lo_addr=(uint32_t)(0x10+bar*4);
    uint32_t hi_addr=(uint32_t)(lo_addr+4);
    uint32_t orig_lo=pci_read_cfg(bus,slot,func,(uint8_t)lo_addr);
    int is64=(!(orig_lo&1))&&((orig_lo&0x4)!=0);
    uint32_t orig_hi=is64?pci_read_cfg(bus,slot,func,(uint8_t)hi_addr):0;
    pci_write_cfg(bus,slot,func,(uint8_t)lo_addr,0xFFFFFFFFu);
    if(is64) pci_write_cfg(bus,slot,func,(uint8_t)hi_addr,0xFFFFFFFFu);
    uint32_t mask_lo=(orig_lo&1)?0xFFFFFFFFu:0xFFFFFFF0u;
    uint32_t lo=pci_read_cfg(bus,slot,func,(uint8_t)lo_addr)&mask_lo;
    uint32_t hi=is64?pci_read_cfg(bus,slot,func,(uint8_t)hi_addr):0;
    pci_write_cfg(bus,slot,func,(uint8_t)lo_addr,orig_lo);
    if(is64) pci_write_cfg(bus,slot,func,(uint8_t)hi_addr,orig_hi);
    uint64_t size=(uint64_t)(~lo)+1u;
    if(is64) size|=((uint64_t)(~hi)+1u)<<32;
    if(!size||size==0xFFFFFFFFFFFFFFFFull) size=4096;
    if(size>0x100000) size=0x100000;
    return size;
}

static void map_bar(uint8_t bus,uint8_t slot,uint8_t func,uint8_t bar){
    uint64_t base=pci_bar_addr(bus,slot,func,bar);
    uint64_t size=pci_bar_size(bus,slot,func,bar);
    if(!base||!size) return;
    for(uint64_t off=0;off<size;off+=4096){
        uint64_t va=base+off;
        paging_map(va,va,PAGE_PRESENT|PAGE_WRITE);
    }
    kprintf("[VIRTIO] mapped BAR%u 0x%llx..0x%llx\n",bar,
            (unsigned long long)base,(unsigned long long)(base+size));
}

static int probe_pci_modern(uint16_t pci_device,uint32_t expect_dev_id){
    pci_dev_info_t info;
    if(!pci_find_device(VIRTIO_PCI_VENDOR,pci_device,&info)) return -1;
    virtio_pci_cap_t common,notify,devcfg;
    int co=find_virtio_cap(info.loc.bus,info.loc.slot,info.loc.func,VIRTIO_PCI_CAP_COMMON_CFG,&common);
    int no=find_virtio_cap(info.loc.bus,info.loc.slot,info.loc.func,VIRTIO_PCI_CAP_NOTIFY_CFG,&notify);
    int dco=find_virtio_cap(info.loc.bus,info.loc.slot,info.loc.func,VIRTIO_PCI_CAP_DEVICE_CFG,&devcfg);
    if(co<0||no<0||dco<0){ kprintf("[VIRTIO] pci %04x missing caps (c=%d n=%d d=%d)\n",pci_device,co,no,dco); return -1; }
    if(common.bar>5||notify.bar>5||devcfg.bar>5){ kprintf("[VIRTIO] pci %04x bad cap bar\n",pci_device); return -1; }
    uint64_t common_base=pci_bar_addr(info.loc.bus,info.loc.slot,info.loc.func,common.bar);
    uint64_t notify_base=pci_bar_addr(info.loc.bus,info.loc.slot,info.loc.func,notify.bar);
    uint64_t devcfg_base=pci_bar_addr(info.loc.bus,info.loc.slot,info.loc.func,devcfg.bar);
    if(!common_base){ kprintf("[VIRTIO] pci %04x common bar unassigned\n",pci_device); return -1; }
    uint32_t cmd=pci_read_cfg16(info.loc.bus,info.loc.slot,info.loc.func,0x04);
    cmd|=(uint16_t)(0x1|0x2|0x4);
    pci_write_cfg(info.loc.bus,info.loc.slot,info.loc.func,0x04,cmd);
    for(uint8_t b=0;b<6;b++){
        if(b==common.bar||b==notify.bar||b==devcfg.bar) map_bar(info.loc.bus,info.loc.slot,info.loc.func,b);
    }
    uint32_t mult=pci_read_cfg(info.loc.bus,info.loc.slot,info.loc.func,(uint8_t)(no+16));
    if(ndev>=MAX_VDEV){ kprintf("[VIRTIO] table full\n"); return -1; }
    virtio_device_t *d=&devs[ndev];
    memset(d,0,sizeof(*d));
    d->transport=VIRTIO_TRANSPORT_PCI;
    d->mmio=NULL;
    d->common=(volatile virtio_pci_common_cfg_t*)(uintptr_t)(common_base+common.offset);
    d->notify=(volatile uint8_t*)(uintptr_t)(notify_base+notify.offset);
    d->devcfg=(volatile uint8_t*)(uintptr_t)(devcfg_base+devcfg.offset);
    d->notify_mult=mult?mult:4;
    d->devcfg_len=devcfg.length;
    d->transitional=(pci_device>=0x1000&&pci_device<=0x103F)?1:0;
    d->irq=info.irq_line;
    d->device_id=expect_dev_id;
    virtio_set_status(d,0);
    uint32_t feat=0;
    for(int w=0;w<2;w++){
        d->common->device_feature_select=(uint32_t)w;
        feat|=(d->common->device_feature<<(w*32));
    }
    d->common->device_feature_select=0;
    const char *n=expect_dev_id==VIRTIO_DEV_NET?"Net":expect_dev_id==VIRTIO_DEV_BLK?"Blk":"?";
    kprintf("[VIRTIO] %s id=%u pci %02x:%02x.%u modern common=0x%llx notify=0x%llx mult=%u feats=0x%x\n",
            n,expect_dev_id,info.loc.bus,info.loc.slot,info.loc.func,
            (unsigned long long)(uintptr_t)d->common,(unsigned long long)(uintptr_t)d->notify,
            d->notify_mult,feat);
    ndev++;
    return 0;
}

static int probe_mmio(void){
    for(int i=0;i<MAX_VDEV;i++){
        if(ndev>=MAX_VDEV) break;
        uint64_t pa=VIRTIO_MMIO_BASE+(uint64_t)i*VIRTIO_MMIO_SIZE;
        paging_map(pa,pa,PAGE_PRESENT|PAGE_WRITE);
        virtio_mmio_t *m=(virtio_mmio_t*)(uintptr_t)pa;
        if(m->magic!=0x74726976) continue;
        devs[ndev].transport=VIRTIO_TRANSPORT_MMIO;
        devs[ndev].mmio=m;
        devs[ndev].device_id=m->device_id;
        devs[ndev].irq=48+i;
        const char *n=m->device_id==VIRTIO_DEV_NET?"Net":m->device_id==VIRTIO_DEV_BLK?"Blk":"?";
        kprintf("[VIRTIO] %s id=%u mmio 0x%08X\n",n,m->device_id,(uint32_t)pa);
        ndev++;
    }
    return 0;
}

void virtio_init(void){
    if(probe_pci_modern(VIRTIO_PCI_DEV_BLK_LEGACY,VIRTIO_DEV_BLK)<0)
        probe_pci_modern(VIRTIO_PCI_DEV_BLK_MODERN,VIRTIO_DEV_BLK);
    if(probe_pci_modern(VIRTIO_PCI_DEV_NET_LEGACY,VIRTIO_DEV_NET)<0)
        probe_pci_modern(VIRTIO_PCI_DEV_NET_MODERN,VIRTIO_DEV_NET);
    probe_mmio();
    kprintf("[VIRTIO] %d devices\n",ndev);
}

virtio_device_t *virtio_find_device(uint32_t device_id){
    for(int i=0;i<ndev;i++) if(devs[i].device_id==device_id) return &devs[i];
    return NULL;
}
