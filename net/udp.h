#ifndef UDP_H
#define UDP_H

#include "../include/types.h"

typedef struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} PACKED udp_hdr_t;

int udp_init(void);
int udp_send(uint32_t dst, uint16_t src_port, uint16_t dst_port, uint8_t *payload, uint16_t len);
int udp_recv(uint8_t *buf, uint16_t max_len, uint32_t *src_ip, uint16_t *src_port);

#endif
