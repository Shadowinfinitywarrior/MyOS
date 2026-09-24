#ifndef ETH_H
#define ETH_H

#include "../include/types.h"

#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

typedef struct eth_header {
    uint8_t  dest[6];
    uint8_t  src[6];
    uint16_t type;
} PACKED eth_header_t;

int eth_init(void);
int eth_send(uint8_t *payload, uint16_t len, uint16_t ethertype);
int eth_recv(uint8_t *buf, uint16_t max_len);

#endif
