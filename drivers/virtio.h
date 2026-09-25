#ifndef VIRTIO_H
#define VIRTIO_H
#include "../include/types.h"
#define VIRTIO_PCI_CAP_COMMON 1
#define VIRTIO_DEV_NET 1
#define VIRTIO_DEV_BLK 2
#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_OK 4
#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2
#define VIRTIO_TRANSPORT_MMIO 0
#define VIRTIO_TRANSPORT_PCI 1
#define VIRTIO_PCI_VENDOR 0x1AF4
#define VIRTIO_PCI_DEV_NET_MODERN 0x1040
#define VIRTIO_PCI_DEV_BLK_MODERN 0x1041
#define VIRTIO_PCI_DEV_NET_LEGACY 0x1000
#define VIRTIO_PCI_DEV_BLK_LEGACY 0x1001
#define VIRTIO_F_VERSION_1 32
#define VIRTIO_BLK_F_SIZE_MAX 1
#define VIRTIO_BLK_F_SEG_MAX 2
typedef struct virtio_pci_cap {
    uint8_t cap_vndr; uint8_t cap_next; uint8_t cap_len; uint8_t cfg_type;
    uint8_t bar; uint8_t padding[3]; uint32_t offset; uint32_t length;
} virtio_pci_cap_t;
typedef struct virtio_pci_common_cfg {
    uint32_t device_feature_select; uint32_t device_feature;
    uint32_t driver_feature_select; uint32_t driver_feature;
    uint16_t msix_config; uint16_t num_queues;
    uint8_t device_status; uint8_t config_generation;
    uint16_t queue_select; uint16_t queue_size; uint16_t queue_msix_vector;
    uint16_t queue_enable; uint16_t queue_notify_off;
    uint64_t queue_desc; uint64_t queue_driver; uint64_t queue_device;
} virtio_pci_common_cfg_t;
typedef struct virtio_mmio {
    uint32_t magic; uint32_t version; uint32_t device_id; uint32_t vendor_id;
    uint32_t device_features; uint32_t device_features_sel;
    uint32_t reserved1[2];
    uint32_t driver_features; uint32_t driver_features_sel;
    uint32_t reserved2[2];
    uint32_t queue_sel; uint32_t queue_num_max; uint32_t queue_num;
    uint32_t reserved3[2]; uint32_t queue_ready;
    uint32_t reserved4[2]; uint32_t queue_notify;
    uint32_t reserved5[3]; uint32_t interrupt_status; uint32_t interrupt_ack;
    uint32_t reserved6[2]; uint32_t status;
    uint32_t reserved7[3];
    uint32_t queue_desc_low; uint32_t queue_desc_high;
    uint32_t reserved8[2]; uint32_t queue_driver_low; uint32_t queue_driver_high;
    uint32_t reserved9[2]; uint32_t queue_device_low; uint32_t queue_device_high;
} virtio_mmio_t;
typedef struct vring_desc { uint64_t addr; uint32_t len; uint16_t flags; uint16_t next; } vring_desc_t;
typedef struct vring_avail { uint16_t flags; uint16_t idx; uint16_t ring[]; } vring_avail_t;
typedef struct vring_used_elem { uint32_t id; uint32_t len; } vring_used_elem_t;
typedef struct vring_used { uint16_t flags; uint16_t idx; vring_used_elem_t ring[]; } vring_used_t;
typedef struct virtqueue {
    uint16_t num; vring_desc_t *volatile desc; vring_avail_t *volatile avail; vring_used_t *volatile used;
    uint16_t free_head; uint16_t num_free; uint16_t last_used_idx;
} virtqueue_t;
typedef struct virtio_device {
    virtio_mmio_t *mmio;
    volatile virtio_pci_common_cfg_t *common;
    volatile uint8_t *notify;
    volatile uint8_t *devcfg;
    uint32_t notify_mult;
    uint32_t devcfg_len;
    uint8_t transitional;
    uint8_t transport;
    uint32_t device_id;
    uint32_t irq;
    virtqueue_t *queues[8];
    int num_queues;
} virtio_device_t;
void virtio_init(void);
virtio_device_t *virtio_find_device(uint32_t device_id);
int virtio_setup_queue(virtio_device_t *dev,int q,uint16_t size);
int virtqueue_add_buf(virtqueue_t *vq,vring_desc_t *sg,uint16_t out,uint16_t in);
void virtqueue_kick(virtio_device_t *dev,int q);
int virtqueue_get_buf(virtqueue_t *vq,uint32_t *len);
void virtqueue_release_buf(virtqueue_t *vq,uint16_t head);
void virtio_device_reset(virtio_device_t *dev);
void virtio_device_ready(virtio_device_t *dev);
uint64_t virtio_get_features(virtio_device_t *dev);
void virtio_set_features(virtio_device_t *dev,uint64_t features);
uint32_t virtio_get_status(virtio_device_t *dev);
void virtio_set_status(virtio_device_t *dev,uint32_t status);
void virtio_read_config(virtio_device_t *dev,uint16_t offset,void *dst,uint32_t len);
int virtio_is_modern(virtio_device_t *dev);
#endif
