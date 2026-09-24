#include "eth.h"
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static uint8_t my_mac[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01};
int eth_init(void) {
    kprintf("[ETH] Initialized\n");
    return 0;
}
int eth_send(uint8_t *payload, uint16_t len, uint16_t ethertype) {
    uint8_t frame[1500];
    eth_header_t *hdr = (eth_header_t *)frame;
    memcpy(hdr->src, my_mac, 6);
    hdr->type = ethertype;
    if (ethertype == ETH_TYPE_IP && len >= sizeof(ip_hdr_t)) {
        ip_hdr_t *ip = (ip_hdr_t *)payload;
        uint8_t mac[6];
        if (arp_resolve(ip->dst_addr, mac) == 0) {
            memcpy(hdr->dest, mac, 6);
        } else {
            memset(hdr->dest, 0xFF, 6);
        }
    } else {
        memset(hdr->dest, 0xFF, 6);
    }
    memcpy(frame + sizeof(eth_header_t), payload, len);
    return net_send(frame, len + sizeof(eth_header_t));
}
int eth_recv(uint8_t *buf, uint16_t max_len) {
    uint8_t frame[1500];
    int len = net_recv(frame, max_len);
    if ((size_t)len < sizeof(eth_header_t)) return 0;
    eth_header_t *hdr = (eth_header_t *)frame;
    uint16_t payload_len = len - sizeof(eth_header_t);
    if (hdr->type == ETH_TYPE_ARP) {
        arp_handle_packet(frame + sizeof(eth_header_t), payload_len);
        return 0;
    }
    memcpy(buf, frame + sizeof(eth_header_t), payload_len);
    return payload_len;
}
