#ifndef MODULE_H
#define MODULE_H
#include "../include/system.h"
void module_init(void);
int module_load(const char *name,const uint8_t *data,uint32_t size);
#endif
