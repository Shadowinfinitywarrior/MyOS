#include "socket.h"
#include "udp.h"
#include "ip.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "byteorder.h"

#define MAX_UDP_SOCKETS 32

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

static udp_socket_t udp_sockets[MAX_UDP_SOCKETS];

static udp_socket_t *udp_socket_alloc(void) {
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

    if (sock->rx_size + len + 8 > sock->rx_capacity) return; // drop

    // Store packet header: src_ip (4) + src_port (2) + len (2)
    uint8_t *hdr = &sock->rx_queue[sock->rx_tail];
    *(uint32_t *)hdr = htonl(0); // placeholder for src_ip
    *(uint16_t *)(hdr + 4) = htons(0); // placeholder for src_port
    *(uint16_t *)(hdr + 6) = htons(len);
    sock->rx_tail = (sock->rx_tail + 8) % sock->rx_capacity;
    sock->rx_size += 8;

    for (uint16_t i = 0; i < len; i++) {
        sock->rx_queue[sock->rx_tail] = data[i];
        sock->rx_tail = (sock->rx_tail + 1) % sock->rx_capacity;
        sock->rx_size++;
    }
}

int socket_init(void) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        udp_sockets[i].in_use = 0;
    }
    return 0;
}

int socket(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET) return -1;
    if (type != SOCK_DGRAM && type != SOCK_STREAM) return -1;

    udp_socket_t *sock = udp_socket_alloc();
    if (!sock) return -1;

    // Find free file descriptor
    // For now return socket index + 3 (stdin=0, stdout=1, stderr=2)
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (&udp_sockets[i] == sock) {
            return i + 3;
        }
    }
    return -1;
}

int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    if (sockfd < 3 || sockfd >= 3 + 32) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= 32 || !udp_sockets[idx].in_use) return -1;

    if (addr->sa_family != AF_INET) return -1;
    if (addrlen < sizeof(sockaddr_in_t)) return -1;

    sockaddr_in_t *sin = (sockaddr_in_t *)addr;
    udp_sockets[sockfd - 3].local_port = sin->sin_port;
    udp_sockets[sockfd - 3].local_ip = sin->sin_addr;
    return 0;
}

int sendto(int sockfd, const void *buf, uint32_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen) {
    (void)flags;
    if (sockfd < 3 || sockfd >= 3 + 32) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= 32 || !udp_sockets[idx].in_use) return -1;

    if (dest_addr->sa_family != AF_INET) return -1;
    if (addrlen < sizeof(sockaddr_in_t)) return -1;

    sockaddr_in_t *sin = (sockaddr_in_t *)dest_addr;
    uint32_t dst_ip = sin->sin_addr;
    uint16_t dst_port = sin->sin_port;
    uint16_t src_port = udp_sockets[sockfd - 3].local_port;

    return udp_send(ntohl(dst_ip), ntohs(src_port), ntohs(dst_port), (uint8_t *)buf, len);
}

int recvfrom(int sockfd, void *buf, uint32_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen) {
    (void)flags;
    if (sockfd < 3 || sockfd >= 3 + 32) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= 32 || !udp_sockets[idx].in_use) return -1;

    udp_socket_t *sock = &udp_sockets[sockfd - 3];
    if (sock->rx_size == 0) return 0; // non-blocking for now

    // Parse header
    if (sock->rx_size < 8) return -1;
    uint8_t hdr[8];
    for (int i = 0; i < 8; i++) {
        hdr[i] = sock->rx_queue[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % sock->rx_capacity;
    }
    sock->rx_size -= 8;

    uint16_t src_port = ntohs(*(uint16_t *)(hdr + 4));
    uint16_t payload_len = ntohs(*(uint16_t *)(hdr + 6));
    if (payload_len > len) payload_len = len;

    // Read payload
    for (uint16_t i = 0; i < payload_len; i++) {
        ((uint8_t *)buf)[i] = sock->rx_queue[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % sock->rx_capacity;
    }
    sock->rx_size -= payload_len;

    if (src_addr && addrlen && *addrlen >= sizeof(sockaddr_in_t)) {
        sockaddr_in_t *sin = (sockaddr_in_t *)src_addr;
        sin->sin_family = AF_INET;
        sin->sin_port = src_port;
        sin->sin_addr = htonl(0);
        *addrlen = sizeof(sockaddr_in_t);
    }

    return payload_len;
}

int close(int fd) {
    if (fd < 3 || fd >= 3 + 32) return -1;
    int idx = fd - 3;
    if (idx < 0 || idx >= 32 || !udp_sockets[idx].in_use) return -1;

    if (udp_sockets[idx].rx_queue) {
        kfree(udp_sockets[idx].rx_queue);
    }
    udp_sockets[idx].in_use = 0;
    return 0;
}