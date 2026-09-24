#include "gdt.h"
#include "idt.h"
#include "../lib/printf.h"
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#pragma GCC diagnostic ignored "-Wunused-parameter"

#define GDT_ENTRIES 7

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} PACKED gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} PACKED gdt_ptr_t;

static gdt_entry_t gdt[GDT_ENTRIES];

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low  = base & 0xFFFF;
    gdt[num].base_mid  = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].gran      = (limit >> 16) & 0x0F;
    gdt[num].gran     |= gran & 0xF0;
    gdt[num].access    = access;
}

void gdt_set_tss(uint64_t base, uint32_t limit) {
    gdt_set_gate(5, base & 0xFFFFFFFF, limit, 0x89, 0x00);
    uint32_t base_high32 = base >> 32;
    *(uint32_t*)&gdt[6] = base_high32;
    *((uint32_t*)&gdt[6] + 1) = 0;
}

void gdt_init(void) {
    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); /* 64-bit kernel code: L=1, D/B=0, Gran=1 */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xC0); /* 64-bit kernel data: Gran=1, D/B=1 */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF2, 0xC0); /* 64-bit user data index 3 (0x18) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xFA, 0xA0); /* 64-bit user code index 4 (0x20) */
    gdt_set_gate(5, 0, 0, 0x89, 0x00);          /* TSS */
    gdt_set_gate(6, 0, 0, 0, 0);
    gdt_ptr_t ptr;
    ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    ptr.base = (uint64_t)&gdt;
    __asm__ __volatile__("lgdt %0" : : "m"(ptr));
    __asm__ __volatile__("mov $0x10, %%ax\n\t"
                         "mov %%ax, %%ds\n\t"
                         "mov %%ax, %%es\n\t"
                         "mov %%ax, %%fs\n\t"
                         "mov %%ax, %%gs\n\t"
                         "mov %%ax, %%ss\n\t"
                         "pushq $0x08\n\t"
                         "lea 1f(%%rip), %%rax\n\t"
                         "pushq %%rax\n\t"
                         "lretq\n\t"
                         "1:\n\t" : : : "rax");
    kprintf("[GDT] 64-bit GDT loaded with %d entries\n", GDT_ENTRIES);
}
