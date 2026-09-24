#include "net.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

net_driver_t *net_current_driver = NULL;

int net_init(void) {
    kprintf("[NET] Stack initialized\n");
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

void net_poll(void) {}
