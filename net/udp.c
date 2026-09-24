#include "udp.h"
#include "ip.h"
#include "../lib/string.h"

int udp_init(void) { return 0; }

static uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip, uint8_t *pkt, uint16_t len) {
    uint32_t sum = 0;
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;
    sum += IPV4_PROTO_UDP;
    sum += len;
    uint16_t *w = (uint16_t *)pkt;
    for (int i = 0; i < len/2; i++) sum += w[i];
    if (len & 1) sum += ((uint8_t *)pkt)[len-1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum & 0xFFFF;
}

int udp_send(uint32_t dst, uint16_t src_port, uint16_t dst_port, uint8_t *payload, uint16_t len) {
    if (!payload || len == 0 || len > 1472) return -1;
    uint8_t pkt[1500];
    udp_hdr_t *hdr = (udp_hdr_t *)pkt;
    hdr->src_port = src_port;
    hdr->dst_port = dst_port;
    hdr->len = sizeof(udp_hdr_t) + len;
    memcpy(pkt + sizeof(udp_hdr_t), payload, len);
    hdr->checksum = udp_checksum(0, dst, pkt, sizeof(udp_hdr_t) + len);
    int ret = ip_send(dst, IPV4_PROTO_UDP, pkt, sizeof(udp_hdr_t) + len);
    return ret < 0 ? -1 : 0;
}

int udp_recv(uint8_t *buf, uint16_t max_len, uint32_t *src_ip, uint16_t *src_port) {
    if (!buf || !src_ip || !src_port) return -1;
    uint8_t pkt[1500];
    int len = ip_recv(pkt, max_len + sizeof(udp_hdr_t), src_ip);
    if ((size_t)len < sizeof(udp_hdr_t)) return -1;
    udp_hdr_t *hdr = (udp_hdr_t *)pkt;
    uint16_t payload_len = hdr->len - sizeof(udp_hdr_t);
    if (payload_len > max_len) payload_len = max_len;
    *src_port = hdr->src_port;
    memcpy(buf, pkt + sizeof(udp_hdr_t), payload_len);
    return payload_len;
}
