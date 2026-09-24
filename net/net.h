#ifndef NET_H
#define NET_H

#include "../include/types.h"

#define NET_BUFSIZE 2048
#define NET_MAX_PACKET 1500

typedef struct net_buf {
    uint8_t  data[NET_MAX_PACKET];
    uint16_t len;
    uint32_t timestamp;
} net_buf_t;

typedef struct net_driver {
    int   (*init)(void);
    int   (*send)(uint8_t *buf, uint16_t len);
    int   (*recv)(uint8_t *buf, uint16_t max_len);
} net_driver_t;

int net_init(void);
int net_send(uint8_t *buf, uint16_t len);
int net_recv(uint8_t *buf, uint16_t max_len);
void net_poll(void);

extern net_driver_t *net_current_driver;

#endif
