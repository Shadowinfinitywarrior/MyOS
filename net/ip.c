#include "ip.h"
#include "eth.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static uint32_t my_ip = 0xC0A80001;

int ip_init(void) { return 0; }

uint16_t ip_checksum(void *buf, uint16_t len) {
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)buf;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

int ip_send(uint32_t dst, uint8_t proto, uint8_t *payload, uint16_t len) {
    uint8_t pkt[1500];
    ip_hdr_t *hdr = (ip_hdr_t *)pkt;
    hdr->ver_ihl = (IPV4_VERSION << 4) | IPV4_IHL;
    hdr->dscp_ecn = IPV4_DSCP;
    hdr->total_len = sizeof(ip_hdr_t) + len;
    hdr->id = 0;
    hdr->flags_offset = 0;
    hdr->ttl = IPV4_TTL;
    hdr->protocol = proto;
    hdr->checksum = 0;
    hdr->src_addr = my_ip;
    hdr->dst_addr = dst;
    hdr->checksum = ip_checksum(hdr, sizeof(ip_hdr_t));
    memcpy(pkt + sizeof(ip_hdr_t), payload, len);
    return eth_send(pkt, hdr->total_len, ETH_TYPE_IP);
}

int ip_recv(uint8_t *buf, uint16_t max_len, uint32_t *src) {
    uint8_t pkt[1500];
    int len = eth_recv(pkt, max_len);
    if ((size_t)len < sizeof(ip_hdr_t)) return 0;
    ip_hdr_t *hdr = (ip_hdr_t *)pkt;
    if (src) *src = hdr->src_addr;
    memcpy(buf, pkt + sizeof(ip_hdr_t), len - sizeof(ip_hdr_t));
    return len - sizeof(ip_hdr_t);
}
