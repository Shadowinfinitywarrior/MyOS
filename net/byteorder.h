#ifndef NET_BYTEORDER_H
#define NET_BYTEORDER_H

#include "../include/types.h"

static inline uint16_t htons(uint16_t x) { return ((x >> 8) & 0xFF) | ((x & 0xFF) << 8); }
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) | ((x & 0xFF00) << 8) | ((x & 0xFF) << 24);
}
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

#endif