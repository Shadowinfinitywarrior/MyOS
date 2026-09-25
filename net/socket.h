#ifndef SOCKET_H
#define SOCKET_H

#include "../include/types.h"

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define AF_INET 2

#define SOL_SOCKET 1
#define SO_REUSEADDR 2

typedef struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
} sockaddr_t;

typedef struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} sockaddr_in_t;

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int sendto(int sockfd, const void *buf, uint32_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen);
int recvfrom(int sockfd, void *buf, uint32_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen);
int close(int fd);

int socket_init(void);

void udp_rx_callback(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, uint8_t *data, uint16_t len);

#endif