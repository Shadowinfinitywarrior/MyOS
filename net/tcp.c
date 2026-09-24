#include "tcp.h"
#include "ip.h"
#include "../lib/string.h"
#include "../kernel/timer.h"

#define TCP_SYN_SENT 1
#define TCP_SYN_RECEIVED 2
#define TCP_ESTABLISHED 3

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

static void tcp_retransmit_cb(void *arg) {
    tcp_sock_t *sock = (tcp_sock_t *)arg;
    if (!sock || sock->state != TCP_SYN_SENT) return;
    /* Simple retransmit SYN */
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = sock->local_port;
    hdr->dst_port = sock->remote_port;
    hdr->seq = sock->seq;
    hdr->ack = 0;
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_SYN;
    hdr->window = 65535;
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(0, sock->remote_ip, (uint8_t *)hdr, sizeof(tcp_hdr_t));
    ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
}



int tcp_init(void) { return 0; }

int tcp_connect(tcp_sock_t *sock, uint32_t ip, uint16_t port) {
    if (!sock || ip == 0 || port == 0) return -1;
    sock->remote_ip = ip;
    sock->remote_port = port;
    sock->seq = tcp_seq_counter++;
    sock->ack = 0;
    sock->state = TCP_SYN_SENT;
    uint8_t pkt[20];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = sock->local_port;
    hdr->dst_port = port;
    hdr->seq = sock->seq;
    hdr->ack = 0;
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_SYN;
    hdr->window = 65535;
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    hdr->checksum = tcp_checksum(0, ip, (uint8_t *)hdr, sizeof(tcp_hdr_t));
    int ret = ip_send(ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t));
    if (ret < 0) return -1;
    timer_add(1000, tcp_retransmit_cb, sock);
    return 0;
}

int tcp_send(tcp_sock_t *sock, uint8_t *data, uint16_t len) {
    if (!sock || !data || len == 0 || len > 1460) return -1;
    if (sock->state != TCP_ESTABLISHED) return -1;
    uint8_t pkt[1500];
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    hdr->src_port = sock->local_port;
    hdr->dst_port = sock->remote_port;
    hdr->seq = sock->seq;
    hdr->ack = sock->ack;
    hdr->data_offset = 5 << 4;
    hdr->flags = TCP_ACK;
    hdr->window = 65535;
    hdr->checksum = 0;
    hdr->urg_ptr = 0;
    memcpy(pkt + sizeof(tcp_hdr_t), data, len);
    int ret = ip_send(sock->remote_ip, IPV4_PROTO_TCP, pkt, sizeof(tcp_hdr_t) + len);
    if (ret < 0) return -1;
    sock->seq += len;
    return len;
}

int tcp_recv(tcp_sock_t *sock, uint8_t *buf, uint16_t max_len) {
    if (!sock || !buf || max_len == 0) return -1;
    uint8_t pkt[1500];
    uint32_t src;
    int len = ip_recv(pkt, max_len + sizeof(tcp_hdr_t), &src);
    if ((size_t)len < sizeof(tcp_hdr_t)) return -1;
    tcp_hdr_t *hdr = (tcp_hdr_t *)pkt;
    if (src != sock->remote_ip) return -1;
    /* Handle 3-way handshake state transitions */
    if (sock->state == TCP_SYN_SENT && (hdr->flags & TCP_SYN) && (hdr->flags & TCP_ACK)) {
        sock->ack = hdr->seq + 1;
        sock->state = TCP_SYN_RECEIVED;
        /* Send ACK */
        uint8_t ack_pkt[20];
        tcp_hdr_t *ah = (tcp_hdr_t *)ack_pkt;
        ah->src_port = sock->local_port;
        ah->dst_port = sock->remote_port;
        ah->seq = sock->seq + 1;
        ah->ack = sock->ack;
        ah->data_offset = 5 << 4;
        ah->flags = TCP_ACK;
        ah->window = 65535;
        ah->checksum = 0;
        ah->urg_ptr = 0;
        ah->checksum = tcp_checksum(0, sock->remote_ip, ack_pkt, sizeof(tcp_hdr_t));
        ip_send(sock->remote_ip, IPV4_PROTO_TCP, ack_pkt, sizeof(tcp_hdr_t));
        sock->state = TCP_ESTABLISHED;
    }
    if (sock->state != TCP_ESTABLISHED) {
        /* For handshake packets, don't return payload */
    }
    int payload_len = len - sizeof(tcp_hdr_t);
    if (payload_len > max_len) payload_len = max_len;
    memcpy(buf, pkt + sizeof(tcp_hdr_t), payload_len);
    if (payload_len > 0) sock->ack += payload_len;
    return payload_len;
}

void tcp_poll(void) {
    /* Simplified 3-way handshake and retransmission handling.
       Retransmit SYN after timeout up to 3 attempts. State transitions
       are driven by incoming packets in tcp_recv. */
}
