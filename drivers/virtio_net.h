#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include "../include/types.h"

int virtio_net_init(void);
int virtio_net_send(uint8_t *data, uint16_t len);
int virtio_net_receive(uint8_t *buf, uint16_t max_len);
const uint8_t *virtio_net_mac(void);

#endif