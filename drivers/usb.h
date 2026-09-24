#ifndef USB_H
#define USB_H
#include "../include/types.h"
typedef struct { uint16_t vid; uint16_t pid; uint8_t class; } usb_dev_t;
int usb_init(void);
int usb_probe(usb_dev_t* d);
#endif
