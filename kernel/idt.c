#include "idt.h"
#include "../include/system.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define IDT_ENTRIES 256

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t zero;
} PACKED idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} PACKED idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t sel, uint8_t flags) {
    idt[num].base_low = handler & 0xFFFF;
    idt[num].base_mid = (handler >> 16) & 0xFFFF;
    idt[num].base_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].zero = 0;
}

void idt_load(void) {
    idt_ptr_t ptr;
    ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    ptr.base = (uint64_t)idt;
    __asm__ __volatile__("lidt %0" : : "m"(ptr));
}

void idt_init(void) {
    extern uint64_t isr_stub_table[IDT_ENTRIES];

    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8E);

    idt_load();
    kprintf("[IDT] Initialized with 256 gates (64-bit)\n");
}