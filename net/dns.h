#ifndef DNS_H
#define DNS_H
#include "../include/system.h"

#include "net.h"

#define DNS_PORT 53

int  dns_resolve(const char *hostname, uint32_t *out_ip);
void dns_set_server(uint32_t ip);

#endif
