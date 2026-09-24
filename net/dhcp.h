#ifndef DHCP_H
#define DHCP_H
#include "../include/system.h"

#include "net.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

void dhcp_init(void);
int  dhcp_discover(void);
int  dhcp_renew(void);
uint32_t dhcp_get_ip(void);

#endif
