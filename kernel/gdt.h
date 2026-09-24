#ifndef GDT_H
#define GDT_H

#include "../include/types.h"

void gdt_init(void);
void gdt_set_tss(uint64_t base, uint32_t limit);
void tss_init(void);
void idt_init_64(void);

#endif
