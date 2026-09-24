#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H
#include "../include/system.h"
int virtio_net_init(void);
int virtio_net_send(uint8_t *data, uint16_t len);
int virtio_net_receive(uint8_t *buf, uint16_t max_len);
#endif
