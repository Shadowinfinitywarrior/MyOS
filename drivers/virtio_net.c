#include "virtio_net.h"
#include "virtio.h"
#include "../lib/printf.h"
#include "../net/net.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static virtio_device_t *dev=NULL;
static net_driver_t virtio_net_driver;
static uint8_t rx_buf[2048];
int virtio_net_init(void){
    dev=virtio_find_device(VIRTIO_DEV_NET);
    if(!dev){ kprintf("[VIRTIO-NET] no device\n"); return -1; }
    virtio_device_reset(dev);
    dev->mmio->status=1|2;
    virtio_setup_queue(dev,0,16);
    virtio_setup_queue(dev,1,16);
    virtio_device_ready(dev);
    virtio_net_driver.init=virtio_net_init;
    virtio_net_driver.send=virtio_net_send;
    virtio_net_driver.recv=virtio_net_receive;
    net_current_driver=&virtio_net_driver;
    virtqueue_t *rvq=dev->queues[1];
    vring_desc_t sg;
    sg.addr=(uint64_t)rx_buf;
    sg.len=sizeof(rx_buf);
    sg.flags=VRING_DESC_F_WRITE;
    virtqueue_add_buf(rvq,&sg,0,1);
    virtqueue_kick(dev,1);
    kprintf("[VIRTIO-NET] init\n");
    return 0;
}
int virtio_net_send(uint8_t *data,uint16_t len){
    if(!dev || !dev->queues[0]) return -1;
    virtqueue_t *vq=dev->queues[0];
    vring_desc_t sg;
    sg.addr=(uint64_t)data;
    sg.len=len;
    sg.flags=0;
    virtqueue_add_buf(vq,&sg,1,0);
    virtqueue_kick(dev,0);
    return len;
}
int virtio_net_receive(uint8_t *buf,uint16_t max_len){
    if(!dev || !dev->queues[1]) return 0;
    virtqueue_t *vq=dev->queues[1];
    uint32_t len=0;
    int id=virtqueue_get_buf(vq,&len);
    if(id<0) return 0;
    if(len>max_len) len=max_len;
    for(uint16_t i=0;i<len;i++) buf[i]=rx_buf[i];
    vring_desc_t sg;
    sg.addr=(uint64_t)rx_buf;
    sg.len=sizeof(rx_buf);
    sg.flags=VRING_DESC_F_WRITE;
    virtqueue_add_buf(vq,&sg,0,1);
    virtqueue_kick(dev,1);
    return len;
}
