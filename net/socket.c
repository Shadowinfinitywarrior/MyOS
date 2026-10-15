#include "socket.h"
#include "udp.h"
#include "tcp.h"
#include "ip.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "byteorder.h"

#define MAX_UDP_SOCKETS 32
#define MAX_FD_SOCKETS  64

typedef enum {
    SK_NONE = 0,
    SK_UDP = 1,
    SK_TCP = 2
} sock_type_t;

typedef struct udp_socket {
    int in_use;
    uint16_t local_port;
    uint32_t local_ip;
    uint8_t *rx_queue;
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_size;
    uint32_t rx_capacity;
} udp_socket_t;

typedef struct sock_desc {
    sock_type_t type;
    int internal_id; /* index in udp_sockets or tcp descriptor */
} sock_desc_t;

static udp_socket_t udp_sockets[MAX_UDP_SOCKETS];
static sock_desc_t sock_table[MAX_FD_SOCKETS];

int socket_init(void) {
    memset(udp_sockets, 0, sizeof(udp_sockets));
    memset(sock_table, 0, sizeof(sock_table));
    return 0;
}

static udp_socket_t *udp_socket_alloc(int *out_idx) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (!udp_sockets[i].in_use) {
            udp_sockets[i].in_use = 1;
            udp_sockets[i].local_port = 0;
            udp_sockets[i].local_ip = 0;
            udp_sockets[i].rx_queue = NULL;
            udp_sockets[i].rx_head = 0;
            udp_sockets[i].rx_tail = 0;
            udp_sockets[i].rx_size = 0;
            udp_sockets[i].rx_capacity = 0;
            if (out_idx) *out_idx = i;
            return &udp_sockets[i];
        }
    }
    return NULL;
}

static udp_socket_t *udp_socket_find_by_port(uint16_t port) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (udp_sockets[i].in_use && udp_sockets[i].local_port == port) {
            return &udp_sockets[i];
        }
    }
    return NULL;
}

void udp_rx_callback(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, uint8_t *data, uint16_t len) {
    (void)src_ip;
    (void)src_port;
    udp_socket_t *sock = udp_socket_find_by_port(dst_port);
    if (!sock) return;

    if (!sock->rx_queue) {
        sock->rx_capacity = 8192;
        sock->rx_queue = (uint8_t *)kmalloc(sock->rx_capacity);
        if (!sock->rx_queue) return;
        sock->rx_head = sock->rx_tail = sock->rx_size = 0;
    }

    if (sock->rx_size + len + 8 > sock->rx_capacity) return;

    uint8_t *hdr = &sock->rx_queue[sock->rx_tail];
    *(uint32_t *)hdr = htonl(src_ip);
    *(uint16_t *)(hdr + 4) = htons(src_port);
    *(uint16_t *)(hdr + 6) = htons(len);
    sock->rx_tail = (sock->rx_tail + 8) % sock->rx_capacity;

    for (uint16_t i = 0; i < len; i++) {
        sock->rx_queue[sock->rx_tail] = data[i];
        sock->rx_tail = (sock->rx_tail + 1) % sock->rx_capacity;
    }
    sock->rx_size += 8 + len;
}

int socket(int domain, int type, int protocol) {
    (void)domain;
    (void)protocol;

    int free_fd = -1;
    for (int i = 3; i < MAX_FD_SOCKETS; i++) {
        if (sock_table[i].type == SK_NONE) {
            free_fd = i;
            break;
        }
    }
    if (free_fd < 0) return -1;

    if (type == SOCK_DGRAM) {
        int u_idx = -1;
        udp_socket_t *u = udp_socket_alloc(&u_idx);
        if (!u) return -1;
        sock_table[free_fd].type = SK_UDP;
        sock_table[free_fd].internal_id = u_idx;
        return free_fd;
    } else if (type == SOCK_STREAM) {
        int tfd = tcp_socket();
        if (tfd < 0) return -1;
        sock_table[free_fd].type = SK_TCP;
        sock_table[free_fd].internal_id = tfd;
        return free_fd;
    }
    return -1;
}

int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type == SK_NONE) return -1;
    if (addr->sa_family != AF_INET || addrlen < sizeof(sockaddr_in_t)) return -1;

    sockaddr_in_t *sin = (sockaddr_in_t *)addr;
    uint16_t port = ntohs(sin->sin_port);

    if (sock_table[sockfd].type == SK_UDP) {
        int idx = sock_table[sockfd].internal_id;
        if (idx < 0 || idx >= MAX_UDP_SOCKETS || !udp_sockets[idx].in_use) return -1;
        udp_sockets[idx].local_port = port;
        return 0;
    } else if (sock_table[sockfd].type == SK_TCP) {
        return tcp_bind(sock_table[sockfd].internal_id, ntohl(sin->sin_addr), port);
    }
    return -1;
}

int listen(int sockfd, int backlog) {
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type != SK_TCP) return -1;
    return tcp_listen(sock_table[sockfd].internal_id, backlog);
}

int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen) {
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type != SK_TCP) return -1;
    int accepted_tcp = tcp_accept(sock_table[sockfd].internal_id, addr, addrlen);
    if (accepted_tcp < 0) return -1;

    for (int i = 3; i < MAX_FD_SOCKETS; i++) {
        if (sock_table[i].type == SK_NONE) {
            sock_table[i].type = SK_TCP;
            sock_table[i].internal_id = accepted_tcp;
            return i;
        }
    }
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type != SK_TCP) return -1;
    if (addr->sa_family != AF_INET || addrlen < sizeof(sockaddr_in_t)) return -1;

    sockaddr_in_t *sin = (sockaddr_in_t *)addr;
    return tcp_connect(sock_table[sockfd].internal_id, ntohl(sin->sin_addr), ntohs(sin->sin_port));
}

int send(int sockfd, const void *buf, size_t len, int flags) {
    (void)flags;
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type == SK_TCP) {
        return tcp_send(sock_table[sockfd].internal_id, buf, (uint16_t)len);
    }
    return -1;
}

int recv(int sockfd, void *buf, size_t len, int flags) {
    (void)flags;
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type == SK_TCP) {
        return tcp_recv(sock_table[sockfd].internal_id, buf, (uint16_t)len);
    }
    return -1;
}

int sendto(int sockfd, const void *buf, uint32_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen) {
    (void)flags;
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type == SK_TCP) {
        return send(sockfd, buf, len, flags);
    }
    if (sock_table[sockfd].type != SK_UDP) return -1;

    int idx = sock_table[sockfd].internal_id;
    if (idx < 0 || idx >= MAX_UDP_SOCKETS || !udp_sockets[idx].in_use) return -1;
    if (!dest_addr || dest_addr->sa_family != AF_INET || addrlen < sizeof(sockaddr_in_t)) return -1;

    sockaddr_in_t *sin = (sockaddr_in_t *)dest_addr;
    uint32_t dst_ip = sin->sin_addr;
    uint16_t dst_port = sin->sin_port;
    uint16_t src_port = udp_sockets[idx].local_port;

    return udp_send(ntohl(dst_ip), ntohs(src_port), ntohs(dst_port), (uint8_t *)buf, len);
}

int recvfrom(int sockfd, void *buf, uint32_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen) {
    (void)flags;
    if (sockfd < 3 || sockfd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[sockfd].type == SK_TCP) {
        return recv(sockfd, buf, len, flags);
    }
    if (sock_table[sockfd].type != SK_UDP) return -1;

    int idx = sock_table[sockfd].internal_id;
    if (idx < 0 || idx >= MAX_UDP_SOCKETS || !udp_sockets[idx].in_use) return -1;

    udp_socket_t *sock = &udp_sockets[idx];
    if (sock->rx_size == 0) return 0;

    if (sock->rx_size < 8) return -1;
    uint8_t hdr[8];
    for (int i = 0; i < 8; i++) {
        hdr[i] = sock->rx_queue[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % sock->rx_capacity;
    }
    sock->rx_size -= 8;

    uint32_t src_ip = ntohl(*(uint32_t *)hdr);
    uint16_t src_port = ntohs(*(uint16_t *)(hdr + 4));
    uint16_t payload_len = ntohs(*(uint16_t *)(hdr + 6));
    if (payload_len > len) payload_len = len;

    for (uint16_t i = 0; i < payload_len; i++) {
        ((uint8_t *)buf)[i] = sock->rx_queue[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % sock->rx_capacity;
    }
    sock->rx_size -= payload_len;

    if (src_addr && addrlen && *addrlen >= sizeof(sockaddr_in_t)) {
        sockaddr_in_t *sin = (sockaddr_in_t *)src_addr;
        sin->sin_family = AF_INET;
        sin->sin_port = htons(src_port);
        sin->sin_addr = htonl(src_ip);
        *addrlen = sizeof(sockaddr_in_t);
    }

    return payload_len;
}

int shutdown(int sockfd, int how) {
    (void)how;
    return close(sockfd);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    (void)sockfd; (void)level; (void)optname;
    if (optval && optlen && *optlen >= sizeof(int)) {
        *(int *)optval = 1;
        *optlen = sizeof(int);
    }
    return 0;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    (void)timeout;
    int ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        int fd = fds[i].fd;
        if (fd >= 3 && fd < MAX_FD_SOCKETS && sock_table[fd].type != SK_NONE) {
            fds[i].revents = (fds[i].events & (POLLIN | POLLOUT));
            ready++;
        }
    }
    return ready;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    (void)exceptfds; (void)timeout;
    int count = 0;
    for (int i = 0; i < nfds && i < MAX_FD_SOCKETS; i++) {
        if (sock_table[i].type != SK_NONE) {
            count++;
        } else {
            if (readfds) readfds->fds_bits[i / 64] &= ~(1ULL << (i % 64));
            if (writefds) writefds->fds_bits[i / 64] &= ~(1ULL << (i % 64));
        }
    }
    return count;
}

int close(int fd) {
    if (fd < 3 || fd >= MAX_FD_SOCKETS) return -1;
    if (sock_table[fd].type == SK_UDP) {
        int idx = sock_table[fd].internal_id;
        if (idx >= 0 && idx < MAX_UDP_SOCKETS && udp_sockets[idx].in_use) {
            if (udp_sockets[idx].rx_queue) {
                kfree(udp_sockets[idx].rx_queue);
            }
            udp_sockets[idx].in_use = 0;
        }
    } else if (sock_table[fd].type == SK_TCP) {
        tcp_close(sock_table[fd].internal_id);
    }
    sock_table[fd].type = SK_NONE;
    sock_table[fd].internal_id = 0;
    return 0;
}