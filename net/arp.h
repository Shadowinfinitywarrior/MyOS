#ifndef ARP_H
#define ARP_H

#include "../include/types.h"

#define ARP_REQUEST 1
#define ARP_REPLY   2
#define ARP_HW_ETH  1
#define ARP_PROTO_IP 0x0800

typedef struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} PACKED arp_header_t;

int arp_init(void);
int arp_resolve(uint32_t ip, uint8_t *mac);
int arp_send_request(uint32_t ip);
int arp_handle_packet(uint8_t *buf, uint16_t len);

#endif
