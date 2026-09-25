#include "icmp.h"
#include "ip.h"
#include "../lib/string.h"

static uint16_t icmp_checksum(void *buf, uint16_t len) {
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)buf;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

int icmp_init(void) { return 0; }

int icmp_send_echo(uint32_t dst, uint16_t id, uint16_t seq) {
    uint8_t pkt[64];
    icmp_hdr_t *hdr = (icmp_hdr_t *)pkt;
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = id;
    hdr->seq = seq;
    hdr->checksum = icmp_checksum(hdr, sizeof(icmp_hdr_t));
    return ip_send(dst, IPV4_PROTO_ICMP, pkt, sizeof(icmp_hdr_t));
}

int icmp_handle_packet(uint8_t *buf, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_hdr_t)) return 0;
    icmp_hdr_t *hdr = (icmp_hdr_t *)buf;
    if (hdr->type == ICMP_ECHO_REQUEST) {
        icmp_hdr_t reply;
        reply.type = ICMP_ECHO_REPLY;
        reply.code = 0;
        reply.id = hdr->id;
        reply.seq = hdr->seq;
        reply.checksum = 0;
        reply.checksum = icmp_checksum(&reply, sizeof(icmp_hdr_t));
        return ip_send(src_ip, IPV4_PROTO_ICMP, (uint8_t *)&reply, sizeof(icmp_hdr_t));
    }
    return 0;
}
