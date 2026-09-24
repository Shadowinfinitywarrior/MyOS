#include "arp.h"
#include "eth.h"
#include "net.h"
#include "../lib/string.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static uint8_t my_mac[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01};
static uint32_t my_ip = 0xC0A80001;
#define ARP_TABLE_SIZE 32
#define ARP_TIMEOUT_MS 30000
typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t last_seen;
    uint8_t valid;
} arp_entry_t;
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static void arp_update(uint32_t ip, uint8_t *mac) {
    uint32_t now = timer_get_ms();
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].last_seen = now;
            return;
        }
    }
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip = ip;
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].last_seen = now;
            arp_table[i].valid = 1;
            return;
        }
    }
}
int arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        arp_table[i].valid = 0;
    }
    return 0;
}
int arp_resolve(uint32_t ip, uint8_t *mac) {
    uint32_t now = timer_get_ms();
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            uint32_t age = now - arp_table[i].last_seen;
            if (age <= ARP_TIMEOUT_MS) {
                memcpy(mac, arp_table[i].mac, 6);
                arp_table[i].last_seen = now;
                return 0;
            } else {
                arp_table[i].valid = 0;
            }
        }
    }
    arp_send_request(ip);
    return -1;
}
int arp_send_request(uint32_t ip) {
    uint8_t pkt[28];
    arp_header_t *arp = (arp_header_t *)pkt;
    arp->hw_type = ARP_HW_ETH;
    arp->proto_type = ARP_PROTO_IP;
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = ARP_REQUEST;
    memcpy(arp->sender_mac, my_mac, 6);
    arp->sender_ip = my_ip;
    memset(arp->target_mac, 0, 6);
    arp->target_ip = ip;
    eth_send(pkt, sizeof(arp_header_t), ETH_TYPE_ARP);
    return 0;
}
int arp_handle_packet(uint8_t *buf, uint16_t len) {
    if (len < sizeof(arp_header_t)) return 0;
    arp_header_t *arp = (arp_header_t *)buf;
    if (arp->hw_type != ARP_HW_ETH || arp->proto_type != ARP_PROTO_IP) return 0;
    if (arp->sender_ip != 0) {
        arp_update(arp->sender_ip, arp->sender_mac);
    }
    if (arp->opcode == ARP_REQUEST && arp->target_ip == my_ip) {
        uint8_t frame[14 + sizeof(arp_header_t)];
        eth_header_t *eth = (eth_header_t *)frame;
        memcpy(eth->dest, arp->sender_mac, 6);
        memcpy(eth->src, my_mac, 6);
        eth->type = ETH_TYPE_ARP;
        arp_header_t *rep = (arp_header_t *)(frame + sizeof(eth_header_t));
        rep->hw_type = ARP_HW_ETH;
        rep->proto_type = ARP_PROTO_IP;
        rep->hw_len = 6;
        rep->proto_len = 4;
        rep->opcode = ARP_REPLY;
        memcpy(rep->sender_mac, my_mac, 6);
        rep->sender_ip = my_ip;
        memcpy(rep->target_mac, arp->sender_mac, 6);
        rep->target_ip = arp->sender_ip;
        net_send(frame, sizeof(eth_header_t) + sizeof(arp_header_t));
    }
    return 0;
}
