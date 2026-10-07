#pragma once
#include "../include/system.h"
#include "../include/types.h"

typedef struct {
    uint64_t mmio_base;
    uint32_t mac[2];
    int link_up;
} e1000_t;

int e1000_init(void);
int e1000_send(const void *pkt, size_t len);
int e1000_recv(void *pkt, size_t *len);
