#include "tcp.h"
#include "ip.h"
#include "socket.h"
#include "../lib/string.h"
#include "../kernel/timer.h"
#include "../kernel/heap.h"
#include "byteorder.h"

#define TCP_MAX_SOCKETS 32
#define TCP_MAX_RETRANSMIT 5

typedef enum {
    TCP_CLOSED = 0,
    TCP_LISTEN = 1,
    TCP_SYN_SENT = 2,
    TCP_SYN_RECEIVED = 3,
    TCP_ESTABLISHED = 4,
    TCP_FIN_WAIT_1 = 5,
    TCP_FIN_WAIT_2 = 6,
    TCP_CLOSE_WAIT = 7,
    TCP_CLOSING = 8,
    TCP_LAST_ACK = 8,
    TCP_TIME_WAIT = 9,
} tcp_state_t;

typedef struct tcp_sock_internal {
    int in_use;
    uint16_t local_port;
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t seq;
    uint32_t ack;
    uint32_t snd_una;    // oldest unacknowledged
    uint32_t snd_nxt;    // next sequence to send
    uint32_t rcv_nxt;    // next sequence expected
    uint16_t snd_wnd;
    uint16_t rcv_wnd;
    tcp_state_t state;
    int retransmit_count;
    uint32_t iss;        // initial send sequence
    uint32_t irs;        // initial receive sequence
    uint32_t last_ack_time;
    int accept_queue_head;
    int accept_queue_tail;
    int pending_sockets[8];
} tcp_sock_internal_t;

static tcp_sock_internal_t tcp_sockets[TCP_MAX_SOCKETS];
static uint32_t tcp_seq_counter = 1;

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, uint8_t *pkt, uint16_t len) {
    uint32_t sum = 0;
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;
    sum += IPV4_PROTO_TCP;
    sum += len;
    uint16_t *w = (uint16_t *)pkt;
    for (int i = 0; i < len/2; i++) {
        sum += w[i];
    }
    if (len & 1) {
        sum += ((uint8_t *)pkt)[len-1] << 8;
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum & 0xFFFF;
}

static tcp_sock_internal_t *tcp_socket_alloc(void) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].in_use) {
            tcp_sockets[i].in_use = 1;
            tcp_sockets[i].local_port = 0;
            tcp_sockets[i].local_ip = 0;
            tcp_sockets[i].remote_ip = 0;
            tcp_sockets[i].remote_port = 0;
            tcp_sockets[i].seq = 0;
            tcp_sockets[i].ack = 0;
            tcp_sockets[i].snd_una = 0;
            tcp_sockets[i].snd_nxt = 0;
            tcp_sockets[i].rcv_nxt = 0;
            tcp_sockets[i].snd_wnd = 65535;
            tcp_sockets[i].rcv_wnd = 65535;
            tcp_sockets[i].state = TCP_CLOSED;
            tcp_sockets[i].retransmit_count = 0;
            tcp_sockets[i].iss = 0;
            tcp_sockets[i].irs = 0;
            tcp_sockets[i].last_ack_time = 0;
            tcp_sockets[i].accept_queue_head = 0;
            tcp_sockets[i].accept_queue_tail = 0;
            return &tcp_sockets[i];
        }
    }
    return NULL;
}

static void tcp_send_syn(tcp_sock_internal_t *sock, uint32_t seq) {
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(seq);
    hdr->ack = 0;
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_SYN;
    hdr->window = htons(sock->rcv_wnd);
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(sock->local_ip, sock->remote_ip, pkt, sizeof(tcp_hdr_t));
    ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
}

static void tcp_send_synack(tcp_sock_internal_t *sock) {
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(sock->seq);
    hdr->ack = htonl(sock->ack);
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_SYN | TCP_ACK;
    hdr->window = htons(sock->rcv_wnd);
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(sock->local_ip, sock->remote_ip, pkt, sizeof(tcp_hdr_t));
    ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
}

static void tcp_send_ack(tcp_sock_internal_t *sock) {
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(sock->seq);
    hdr->ack = htonl(sock->ack);
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_ACK;
    hdr->window = htons(sock->rcv_wnd);
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(sock->local_ip, sock->remote_ip, pkt, sizeof(tcp_hdr_t));
    ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
}

static void tcp_send_fin(tcp_sock_internal_t *sock) {
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(sock->seq);
    hdr->ack = htonl(sock->ack);
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_FIN | TCP_ACK;
    hdr->window = htons(sock->rcv_wnd);
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(sock->local_ip, sock->remote_ip, pkt, sizeof(tcp_hdr_t));
    ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
}

static void tcp_retransmit_cb(void *arg) {
    tcp_sock_internal_t *sock = (tcp_sock_internal_t *)arg;
    if (!sock || !sock->in_use) return;
    
    if (sock->state == TCP_SYN_SENT || sock->state == TCP_SYN_RECEIVED) {
        if (sock->retransmit_count < TCP_MAX_RETRANSMIT) {
            sock->retransmit_count++;
            if (sock->state == TCP_SYN_SENT) {
                tcp_send_syn(sock, sock->iss);
            } else {
                tcp_send_synack(sock);
            }
            timer_add(1000 * (1 << sock->retransmit_count), tcp_retransmit_cb, sock);
        }
    }
}

int tcp_init(void) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_sockets[i].in_use = 0;
    }
    return 0;
}

int tcp_socket(void) {
    tcp_sock_internal_t *sock = tcp_socket_alloc();
    if (!sock) return -1;
    sock->state = TCP_CLOSED;
    // Return index as socket descriptor (offset by 3 like UDP)
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (&tcp_sockets[i] == sock) {
            return i + 3;
        }
    }
    return -1;
}

int tcp_bind(int sockfd, uint32_t ip, uint16_t port) {
    (void)ip;
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    sock->local_ip = ntohl(0); // INADDR_ANY
    sock->local_port = ntohs(port);
    return 0;
}

int tcp_listen(int sockfd, int backlog) {
    (void)backlog;
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    if (sock->state != TCP_CLOSED) return -1;
    sock->state = TCP_LISTEN;
    return 0;
}

int tcp_accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen) {
    (void)addr;
    (void)addrlen;
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *listen_sock = &tcp_sockets[idx];
    if (listen_sock->state != TCP_LISTEN) return -1;
    
    // Check accept queue
    if (listen_sock->accept_queue_head == listen_sock->accept_queue_tail) {
        return -1; // no pending connections
    }
    
    int child_idx = listen_sock->pending_sockets[listen_sock->accept_queue_head];
    listen_sock->accept_queue_head = (listen_sock->accept_queue_head + 1) % 8;
    
    tcp_sock_internal_t *child = &tcp_sockets[child_idx];
    child->in_use = 1;
    child->state = TCP_ESTABLISHED;
    
    // Return new socket fd
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (&tcp_sockets[i] == child) {
            return i + 3;
        }
    }
    return -1;
}

int tcp_connect(int sockfd, uint32_t ip, uint16_t port) {
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    if (sock->state != TCP_CLOSED) return -1;
    
    sock->remote_ip = ntohl(ip);
    sock->remote_port = ntohs(port);
    sock->iss = tcp_seq_counter++;
    sock->seq = sock->iss;
    sock->ack = 0;
    sock->snd_una = sock->iss;
    sock->snd_nxt = sock->iss + 1;
    sock->rcv_nxt = 0;
    sock->state = TCP_SYN_SENT;
    sock->retransmit_count = 0;
    
    tcp_send_syn(sock, sock->iss);
    timer_add(1000, tcp_retransmit_cb, sock);
    return 0;
}

int tcp_send(int sockfd, const void *data, uint16_t len) {
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    if (sock->state != TCP_ESTABLISHED) return -1;
    
    uint8_t pkt[1500];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(sock->seq);
    hdr->ack = htonl(sock->ack);
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_ACK;
    hdr->window = htons(sock->rcv_wnd);
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    memcpy(pkt + sizeof(tcp_hdr_t), data, len);
    hdr->checksum = tcp_checksum(sock->local_ip, sock->remote_ip, pkt, sizeof(tcp_hdr_t) + len);
    
    int ret = ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t) + len);
    if (ret < 0) return -1;
    
    sock->seq += len;
    sock->snd_nxt = sock->seq;
    return len;
}

int tcp_recv(int sockfd, void *buf, uint16_t max_len) {
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    if (sock->state != TCP_ESTABLISHED && sock->state != TCP_CLOSE_WAIT) return -1;
    
    uint8_t pkt[1500];
    uint32_t src;
    int len = ip_recv(pkt, sizeof(tcp_hdr_t) + max_len, &src);
    if ((size_t)len < sizeof(tcp_hdr_t)) return 0;
    
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    if (src != sock->remote_ip) return 0;
    if (ntohs(hdr->dst_port) != 0 && ntohs(hdr->dst_port) != sock->local_port) return 0;
    
    uint16_t flags = hdr->flags;
    uint32_t seq = ntohl(hdr->seq);
    uint32_t ack = ntohl(hdr->ack);
    
    // Handle state machine
    if (sock->state == TCP_SYN_SENT) {
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            sock->ack = ntohl(hdr->seq) + 1;
            sock->seq = sock->iss + 1;
            sock->snd_una = sock->iss + 1;
            sock->state = TCP_SYN_RECEIVED;
            tcp_send_ack(sock);
        }
        return 0;
    }
    
    if (sock->state == TCP_SYN_RECEIVED) {
        if (flags & TCP_ACK) {
            sock->state = TCP_ESTABLISHED;
        }
        return 0;
    }
    
    if (sock->state == TCP_ESTABLISHED || sock->state == TCP_CLOSE_WAIT) {
        if (flags & TCP_ACK) {
            // Update send window
            sock->snd_una = ack;
        }
        
        if (flags & TCP_SYN) {
            // Simultaneous open
            sock->ack = seq + 1;
            tcp_send_ack(sock);
        }
        
        if (flags & TCP_FIN) {
            sock->ack = seq + 1;
            tcp_send_ack(sock);
            if (sock->state == TCP_ESTABLISHED) {
                sock->state = TCP_CLOSE_WAIT;
            }
            return 0; // no payload on FIN
        }
        
        int payload_len = len - sizeof(tcp_hdr_t);
        if (payload_len > 0) {
            if (payload_len > max_len) payload_len = max_len;
            memcpy(buf, (uint8_t *)pkt + sizeof(tcp_hdr_t), payload_len);
            sock->ack = seq + payload_len;
            tcp_send_ack(sock);
            return payload_len;
        }
    }
    
    return 0;
}

int tcp_close(int sockfd) {
    if (sockfd < 3 || sockfd >= 3 + TCP_MAX_SOCKETS) return -1;
    int idx = sockfd - 3;
    if (idx < 0 || idx >= TCP_MAX_SOCKETS || !tcp_sockets[idx].in_use) return -1;
    
    tcp_sock_internal_t *sock = &tcp_sockets[idx];
    
    if (sock->state == TCP_ESTABLISHED) {
        sock->state = TCP_FIN_WAIT_1;
        tcp_send_fin(sock);
    } else if (sock->state == TCP_CLOSE_WAIT) {
        sock->state = TCP_LAST_ACK;
        tcp_send_fin(sock);
    } else if (sock->state == TCP_SYN_SENT || sock->state == TCP_SYN_RECEIVED) {
        sock->state = TCP_CLOSED;
        sock->in_use = 0;
    } else if (sock->state == TCP_LISTEN) {
        sock->state = TCP_CLOSED;
        sock->in_use = 0;
    }
    return 0;
}

void tcp_poll(void) {
    // Check for retransmissions and timeouts
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_sock_internal_t *sock = &tcp_sockets[i];
        if (!sock->in_use) continue;
        
        // Handle TIME_WAIT timeout
        if (sock->state == TCP_TIME_WAIT) {
            // After 2*MSL (60s), transition to CLOSED
            // Simplified: just close after a timeout
            sock->state = TCP_CLOSED;
            sock->in_use = 0;
        }
    }
}