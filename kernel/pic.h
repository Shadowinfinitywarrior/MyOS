#ifndef PIC_H
#define PIC_H

#include "../include/system.h"

void pic_init(void);
void pic_clear_mask(uint8_t irq);
void pic_send_eoi(uint8_t irq);

#endif
