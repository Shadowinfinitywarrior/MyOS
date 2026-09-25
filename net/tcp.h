#ifndef TCP_H
#define TCP_H

#include "../include/types.h"
#include "socket.h"

#define TCP_SYN  0x02
#define TCP_ACK  0x10
#define TCP_FIN  0x01
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_URG  0x20

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

int tcp_init(void);
int tcp_socket(void);
int tcp_bind(int sockfd, uint32_t ip, uint16_t port);
int tcp_listen(int sockfd, int backlog);
int tcp_accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int tcp_connect(int sockfd, uint32_t ip, uint16_t port);
int tcp_send(int sockfd, const void *data, uint16_t len);
int tcp_recv(int sockfd, void *buf, uint16_t max_len);
int tcp_close(int sockfd);
void tcp_poll(void);

#endif