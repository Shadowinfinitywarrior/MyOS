#include "net.h"
#include "eth.h"
#include "ip.h"
#include "icmp.h"
#include "arp.h"
#include "udp.h"
#include "socket.h"
#include "byteorder.h"
#include "../lib/printf.h"
#include "../lib/string.h"

net_driver_t *net_current_driver = NULL;

int net_init(void) {
    kprintf("[NET] Stack initialized\n");
    eth_init();
    arp_init();
    ip_init();
    icmp_init();
    udp_init();
    socket_init();
    return 0;
}

int net_send(uint8_t *buf, uint16_t len) {
    if (net_current_driver && net_current_driver->send) {
        return net_current_driver->send(buf, len);
    }
    return -1;
}

int net_recv(uint8_t *buf, uint16_t max_len) {
    if (net_current_driver && net_current_driver->recv) {
        return net_current_driver->recv(buf, max_len);
    }
    return 0;
}

void net_poll(void) {
    if (!net_current_driver) return;

    uint8_t frame[1500];
    int len = net_recv(frame, sizeof(frame));
    if (len <= 0) return;

    if ((size_t)len < sizeof(eth_header_t)) return;
    eth_header_t *eth = (eth_header_t *)frame;
    uint16_t payload_len = len - sizeof(eth_header_t);
    uint8_t *payload = frame + sizeof(eth_header_t);

    if (eth->type == ETH_TYPE_ARP) {
        arp_handle_packet(payload, payload_len);
        return;
    }

    if (eth->type != ETH_TYPE_IP) return;

    if ((size_t)payload_len < sizeof(ip_hdr_t)) return;
    ip_hdr_t *ip = (ip_hdr_t *)payload;
    uint8_t *ip_payload = payload + sizeof(ip_hdr_t);
    uint16_t ip_payload_len = payload_len - sizeof(ip_hdr_t);

    if (ip->protocol == IPV4_PROTO_ICMP) {
        icmp_handle_packet(ip_payload, ip_payload_len, ip->src_addr);
    } else if (ip->protocol == IPV4_PROTO_UDP) {
        if ((size_t)ip_payload_len >= sizeof(udp_hdr_t)) {
            udp_hdr_t *udp = (udp_hdr_t *)ip_payload;
            uint16_t src_port = ntohs(udp->src_port);
            uint16_t dst_port = ntohs(udp->dst_port);
            uint16_t udp_len = ntohs(udp->len);
            if (udp_len >= sizeof(udp_hdr_t) && (size_t)ip_payload_len >= udp_len) {
                uint8_t *udp_payload = ip_payload + sizeof(udp_hdr_t);
                uint16_t payload_len = udp_len - sizeof(udp_hdr_t);
                // Deliver to socket layer
                extern void udp_rx_callback(uint32_t, uint16_t, uint16_t, uint8_t *, uint16_t);
                udp_rx_callback(ip->src_addr, src_port, dst_port, udp_payload, payload_len);
            }
        }
    }
}
