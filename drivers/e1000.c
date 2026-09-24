#include "e1000.h"
#include <lib/printf.h>

static e1000_t nic;

int e1000_init(void) {
    nic.mmio_base = 0;
    nic.link_up = 0;
    kprintf("[e1000] init stub\n");
    return 0;
}

int e1000_send(const void *pkt, size_t len) {
    return -1;
}

int e1000_recv(void *pkt, size_t *len) {
    return -1;
}
