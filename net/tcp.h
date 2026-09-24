#ifndef TCP_H
#define TCP_H

#include "../include/types.h"

#define TCP_SYN  0x02
#define TCP_ACK  0x10
#define TCP_FIN  0x01

typedef struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
} PACKED tcp_hdr_t;

typedef struct tcp_sock {
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t remote_ip;
    uint32_t seq;
    uint32_t ack;
    uint8_t  state;
} tcp_sock_t;

int tcp_init(void);
int tcp_connect(tcp_sock_t *sock, uint32_t ip, uint16_t port);
int tcp_send(tcp_sock_t *sock, uint8_t *data, uint16_t len);
int tcp_recv(tcp_sock_t *sock, uint8_t *buf, uint16_t max_len);
void tcp_poll(void);

#endif
