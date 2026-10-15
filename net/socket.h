#ifndef SOCKET_H
#define SOCKET_H

#include "../include/types.h"

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10

#define SOL_SOCKET   1
#define SO_REUSEADDR 2
#define SO_KEEPALIVE 9
#define SO_RCVTIMEO  20
#define SO_SNDTIMEO  21

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

typedef uint32_t socklen_t;
typedef uint64_t nfds_t;

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

struct pollfd {
    int fd;
    short events;
    short revents;
};

typedef struct {
    uint64_t fds_bits[16];
} fd_set;

struct timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int send(int sockfd, const void *buf, size_t len, int flags);
int recv(int sockfd, void *buf, size_t len, int flags);
int sendto(int sockfd, const void *buf, uint32_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen);
int recvfrom(int sockfd, void *buf, uint32_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen);
int shutdown(int sockfd, int how);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int close(int fd);

int socket_init(void);
void udp_rx_callback(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, uint8_t *data, uint16_t len);

/* Kernel syscall equivalents */
int sys_socket(int domain, int type, int protocol);
int sys_bind(int fd, struct sockaddr *addr, socklen_t len);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, struct sockaddr *addr, socklen_t *len);
int sys_connect(int fd, struct sockaddr *addr, socklen_t len);
int sys_send(int fd, const void *buf, size_t len, int flags);
int sys_recv(int fd, void *buf, size_t len, int flags);
int sys_sendto(int fd, const void *buf, size_t len, int flags,
               struct sockaddr *dest, socklen_t dlen);
int sys_recvfrom(int fd, void *buf, size_t len, int flags,
                 struct sockaddr *src, socklen_t *slen);
int sys_select(int nfds, fd_set *readfds, fd_set *writefds,
               fd_set *exceptfds, struct timeval *timeout);
int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int sys_shutdown(int fd, int how);
int sys_getsockopt(int fd, int level, int optname, void *optval, socklen_t *len);
int sys_setsockopt(int fd, int level, int optname, const void *optval, socklen_t len);

#endif