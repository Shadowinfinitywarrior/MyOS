#include "tss.h"
#include "gdt.h"
#include "../lib/printf.h"
#include "../include/system.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define TSS_SIZE 104

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} PACKED tss_t;

static tss_t tss;

static inline void load_tss(uint16_t selector) {
    __asm__ __volatile__("ltr %w0" : : "r"(selector));
}

void tss_init(void) {
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = 0x90000 + 8192;
    tss.iomap_base = sizeof(tss);
    gdt_set_tss((uint64_t)&tss, sizeof(tss) - 1);
    kprintf("[TSS] Per-CPU TSS initialized (placeholder rsp0)\n");
    load_tss(0x28);
}

/* Set the kernel stack used when an interrupt transitions from Ring 3 */
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
