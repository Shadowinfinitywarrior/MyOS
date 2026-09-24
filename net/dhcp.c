#include "dhcp.h"
#include "udp.h"
#include "../lib/string.h"

static uint32_t dhcp_ip = 0;
static uint32_t dhcp_server = 0;
static uint32_t dhcp_xid = 0x12345678;
static uint8_t dhcp_mac[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01};

void dhcp_init(void)
{
    dhcp_ip = 0;
    dhcp_server = 0;
    dhcp_xid = 0x12345678;
}

static void dhcp_set_xid(uint8_t *pkt, uint32_t xid)
{
    pkt[4] = (xid >> 24) & 0xFF;
    pkt[5] = (xid >> 16) & 0xFF;
    pkt[6] = (xid >> 8) & 0xFF;
    pkt[7] = xid & 0xFF;
}

static void dhcp_set_chaddr(uint8_t *pkt)
{
    for (int i = 0; i < 6; i++) {
        pkt[28 + i] = dhcp_mac[i];
    }
}

static int dhcp_send_request(uint8_t msg_type, uint32_t requested_ip, uint32_t server_id, uint32_t dst_ip)
{
    uint8_t pkt[548];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 1;
    pkt[1] = 1;
    pkt[2] = 6;
    dhcp_set_xid(pkt, dhcp_xid);
    pkt[10] = 0x80;
    pkt[11] = 0x00;
    dhcp_set_chaddr(pkt);
    pkt[236] = 99;
    pkt[237] = 130;
    pkt[238] = 83;
    pkt[239] = 99;
    int idx = 240;
    pkt[idx++] = 53;
    pkt[idx++] = 1;
    pkt[idx++] = msg_type;
    if (requested_ip) {
        pkt[idx++] = 50;
        pkt[idx++] = 4;
        pkt[idx++] = (requested_ip >> 24) & 0xFF;
        pkt[idx++] = (requested_ip >> 16) & 0xFF;
        pkt[idx++] = (requested_ip >> 8) & 0xFF;
        pkt[idx++] = requested_ip & 0xFF;
    }
    if (server_id) {
        pkt[idx++] = 54;
        pkt[idx++] = 4;
        pkt[idx++] = (server_id >> 24) & 0xFF;
        pkt[idx++] = (server_id >> 16) & 0xFF;
        pkt[idx++] = (server_id >> 8) & 0xFF;
        pkt[idx++] = server_id & 0xFF;
    }
    pkt[idx++] = 255;
    return udp_send(dst_ip, 68, 67, pkt, idx);
}

static uint8_t dhcp_parse_msg_type(uint8_t *pkt, int len)
{
    if (len < 240) return 0;
    int idx = 240;
    while (idx + 1 < len) {
        uint8_t code = pkt[idx];
        if (code == 255) break;
        uint8_t opt_len = pkt[idx + 1];
        if (code == 53 && opt_len >= 1) {
            return pkt[idx + 2];
        }
        idx += 2 + opt_len;
    }
    return 0;
}

static uint32_t dhcp_parse_yiaddr(uint8_t *pkt)
{
    return (pkt[16] << 24) | (pkt[17] << 16) | (pkt[18] << 8) | pkt[19];
}

int dhcp_discover(void)
{
    dhcp_xid++;
    uint8_t pkt[548];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 1;
    pkt[1] = 1;
    pkt[2] = 6;
    dhcp_set_xid(pkt, dhcp_xid);
    pkt[10] = 0x80;
    pkt[11] = 0x00;
    dhcp_set_chaddr(pkt);
    pkt[236] = 99;
    pkt[237] = 130;
    pkt[238] = 83;
    pkt[239] = 99;
    int idx = 240;
    pkt[idx++] = 53;
    pkt[idx++] = 1;
    pkt[idx++] = 1;
    pkt[idx++] = 255;
    if (udp_send(0xFFFFFFFF, 68, 67, pkt, idx) < 0) return -1;
    uint8_t resp[548];
    uint32_t src_ip;
    uint16_t src_port;
    int len = udp_recv(resp, sizeof(resp), &src_ip, &src_port);
    if (len <= 0) return -1;
    if (resp[0] != 2) return -1;
    if (resp[4] != pkt[4] || resp[5] != pkt[5] || resp[6] != pkt[6] || resp[7] != pkt[7]) return -1;
    uint8_t mtype = dhcp_parse_msg_type(resp, len);
    if (mtype != 2) return -1;
    uint32_t offered = dhcp_parse_yiaddr(resp);
    dhcp_server = src_ip;
    dhcp_xid++;
    if (dhcp_send_request(3, offered, dhcp_server, dhcp_server) < 0) return -1;
    len = udp_recv(resp, sizeof(resp), &src_ip, &src_port);
    if (len <= 0) return -1;
    if (resp[0] != 2) return -1;
    mtype = dhcp_parse_msg_type(resp, len);
    if (mtype != 5) return -1;
    uint32_t assigned = dhcp_parse_yiaddr(resp);
    if (assigned == 0) return -1;
    dhcp_ip = assigned;
    return 0;
}

int dhcp_renew(void)
{
    if (dhcp_ip == 0 || dhcp_server == 0) return -1;
    dhcp_xid++;
    if (dhcp_send_request(3, dhcp_ip, dhcp_server, dhcp_server) < 0) return -1;
    uint8_t resp[548];
    uint32_t src_ip;
    uint16_t src_port;
    int len = udp_recv(resp, sizeof(resp), &src_ip, &src_port);
    if (len <= 0) return -1;
    uint8_t mtype = dhcp_parse_msg_type(resp, len);
    if (mtype != 5) return -1;
    uint32_t assigned = dhcp_parse_yiaddr(resp);
    if (assigned != dhcp_ip) return -1;
    return 0;
}

uint32_t dhcp_get_ip(void)
{
    return dhcp_ip;
}
