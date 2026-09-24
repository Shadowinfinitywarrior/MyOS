#ifndef APIC_H
#define APIC_H

#include "../include/types.h"

#define APIC_ID         0x020
#define APIC_VERSION    0x030
#define APIC_TPR        0x080
#define APIC_EOI        0x0B0
#define APIC_LDR        0x0D0
#define APIC_DFR        0x0E0
#define APIC_SVR        0x0F0
#define APIC_ISR_BASE   0x100
#define APIC_TMR_BASE   0x180
#define APIC_IRR_BASE   0x200
#define APIC_ESR        0x280
#define APIC_ICR_LOW    0x300
#define APIC_ICR_HIGH   0x310
#define APIC_LVT_TIMER  0x320
#define APIC_LVT_THERM  0x330
#define APIC_LVT_PERF   0x340
#define APIC_LVT_LINT0  0x350
#define APIC_LVT_LINT1  0x360
#define APIC_LVT_ERR    0x370
#define APIC_TIMER_INIT 0x380
#define APIC_TIMER_CUR  0x390
#define APIC_TIMER_DIV  0x3E0

#define IOAPIC_ID       0x00
#define IOAPIC_VER      0x01
#define IOAPIC_ARB      0x02
#define IOAPIC_REDTBL   0x10

#define APIC_ICR_FIXED      0x00000
#define APIC_ICR_LOWEST     0x00100
#define APIC_ICR_SMI        0x00200
#define APIC_ICR_NMI        0x00400
#define APIC_ICR_INIT       0x00500
#define APIC_ICR_STARTUP    0x00600

bool    apic_detect(void);
void    apic_init(void);
void    apic_eoi(void);
void    apic_send_ipi(uint8_t target, uint32_t vector);
uint8_t apic_get_id(void);

void    ioapic_init(uint32_t ioapic_addr);
void    ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t dest);
void    ioapic_mask(uint8_t irq);
void    ioapic_unmask(uint8_t irq);

#endif
