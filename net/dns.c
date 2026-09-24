#include "dns.h"
#include "udp.h"
#include "../lib/string.h"

static uint32_t dns_server = 0;
static uint16_t dns_id = 0;

void dns_set_server(uint32_t ip) {
    dns_server = ip;
}

int dns_resolve(const char *hostname, uint32_t *out_ip) {
    if (!hostname || !out_ip || dns_server == 0) return -1;
    uint8_t query[512];
    memset(query, 0, sizeof(query));
    uint16_t id = dns_id++;
    query[0] = id >> 8;
    query[1] = id & 0xFF;
    query[2] = 0x01;
    query[3] = 0x00;
    query[4] = 0x00;
    query[5] = 0x01;
    int offset = 12;
    const char *p = hostname;
    while (*p) {
        uint8_t lab_len = 0;
        while (*p && *p != '.' && lab_len < 63) {
            lab_len++;
            p++;
        }
        query[offset++] = lab_len;
        memcpy(query + offset, p - lab_len, lab_len);
        offset += lab_len;
        if (*p == '.') {
            p++;
        }
    }
    query[offset++] = 0;
    query[offset++] = 0x00;
    query[offset++] = 0x01;
    query[offset++] = 0x00;
    query[offset++] = 0x01;
    int ret = udp_send(dns_server, 12345, 53, query, offset);
    if (ret < 0) return -1;
    uint8_t resp[512];
    uint32_t src_ip;
    uint16_t src_port;
    int len = udp_recv(resp, sizeof(resp), &src_ip, &src_port);
    if (len <= 0 || len < 12) return -1;
    if (resp[0] != (id >> 8) || resp[1] != (id & 0xFF)) return -1;
    if ((resp[2] & 0x80) == 0) return -1;
    uint16_t rcode = resp[3] & 0x0F;
    if (rcode != 0) return -1;
    uint16_t ancount = (resp[6] << 8) | resp[7];
    if (ancount == 0) return -1;
    int pos = 12;
    while (pos < len && resp[pos] != 0) {
        pos += resp[pos] + 1;
    }
    if (pos >= len) return -1;
    pos += 5;
    for (int i = 0; i < ancount && pos + 10 <= len; i++) {
        if ((resp[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < len && resp[pos] != 0) {
                pos += resp[pos] + 1;
            }
            if (pos < len && resp[pos] == 0) pos++;
        }
        uint16_t type = (resp[pos] << 8) | resp[pos + 1];
        uint16_t class_ = (resp[pos + 2] << 8) | resp[pos + 3];
        uint16_t rdlen = (resp[pos + 8] << 8) | resp[pos + 9];
        pos += 10;
        if (type == 1 && class_ == 1 && rdlen == 4 && pos + 4 <= len) {
            *out_ip = (resp[pos] << 24) | (resp[pos + 1] << 16) | (resp[pos + 2] << 8) | resp[pos + 3];
            return 0;
        }
        pos += rdlen;
    }
    return -1;
}
