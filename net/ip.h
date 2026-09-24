#ifndef IP_H
#define IP_H

#include "../include/types.h"

#define IPV4_VERSION 4
#define IPV4_IHL     5
#define IPV4_DSCP    0
#define IPV4_TTL     64
#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17

typedef struct ip_hdr {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} PACKED ip_hdr_t;

int ip_init(void);
int ip_send(uint32_t dst, uint8_t proto, uint8_t *payload, uint16_t len);
int ip_recv(uint8_t *buf, uint16_t max_len, uint32_t *src);

#endif
