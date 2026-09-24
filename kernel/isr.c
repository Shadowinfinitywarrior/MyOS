#include "isr.h"
#include "pic.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static isr_handler_t handlers[256];

void isr_init(void) {
    for (int i = 0; i < 256; i++)
        handlers[i] = NULL;
    kprintf("[ISR] Initialized\n");
}

void isr_register_handler(uint8_t vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

void isr_handler(registers_t *regs) {
    uint8_t vector = (uint8_t)regs->int_no;

    /* Ack hardware IRQs FIRST (edge-triggered PIC), then dispatch */
    if (vector >= 32 && vector <= 47)
        pic_send_eoi((uint8_t)(vector - 32));

    if (handlers[vector]) {
        handlers[vector](regs);
    } else if (vector < 32) {
        extern void serial_printf(const char *fmt, ...);
        serial_printf("[ISR] Unhandled Exception %d\n", vector);
        serial_printf("RIP=0x%lx RSP=0x%lx CS=0x%lx err=0x%lx\n",
                      (unsigned long)regs->rip, (unsigned long)regs->rsp,
                      (unsigned long)regs->cs, (unsigned long)regs->err_code);
        if (vector == 14) {
            uint64_t cr2;
            __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
            uint64_t cr3v;
            __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3v));
            serial_printf("CR2=0x%lx CR3=0x%lx\n", (unsigned long)cr2,
                          (unsigned long)cr3v);
            extern void paging_dump_dirs(void);
            paging_dump_dirs();
            extern uint64_t *get_pml4_current(void);
            uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3v & ~0xFFFULL);
            if (pml4) {
                uint64_t i4 = (cr2 >> 39) & 0x1FF, i3 = (cr2 >> 30) & 0x1FF,
                         i2 = (cr2 >> 21) & 0x1FF, i1 = (cr2 >> 12) & 0x1FF;
                serial_printf("PML4[%lu]=0x%lx\n", (unsigned long)i4,
                              (unsigned long)pml4[i4]);
                if (pml4[i4] & 1) {
                    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[i4] & ~0xFFFULL);
                    serial_printf("  PDPT[%lu]=0x%lx\n", (unsigned long)i3,
                                  (unsigned long)pdpt[i3]);
                    if (pdpt[i3] & 1) {
                        uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[i3] & ~0xFFFULL);
                        serial_printf("    PD[%lu]=0x%lx\n", (unsigned long)i2,
                                      (unsigned long)pd[i2]);
                        if ((pd[i2] & 1) && !(pd[i2] & 0x80)) {
                            uint64_t *pt = (uint64_t *)(uintptr_t)(pd[i2] & ~0xFFFULL);
                            serial_printf("      PT[%lu]=0x%lx\n", (unsigned long)i1,
                                          (unsigned long)pt[i1]);
                        }
                    }
                }
            }
            serial_printf("[ISR] Page fault halted\n");
            for (;;) {
                __asm__ __volatile__("hlt");
            }
        }
        extern void kernel_panic(const char *, const char *, int);
        kernel_panic("Unhandled Exception", "isr.c", 0);
    }
}