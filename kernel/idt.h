#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t sel, uint8_t flags);
void idt_load(void);

#endif
