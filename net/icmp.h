#ifndef ICMP_H
#define ICMP_H

#include "../include/types.h"

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct icmp_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} PACKED icmp_hdr_t;

int icmp_init(void);
int icmp_send_echo(uint32_t dst, uint16_t id, uint16_t seq);
int icmp_handle_packet(uint8_t *buf, uint16_t len);

#endif
