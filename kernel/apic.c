#include "apic.h"
#include "paging.h"
#include "../include/system.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static volatile uint32_t *apic_base = NULL;
static volatile uint32_t *ioapic_base_addr = NULL;

#define MSR_APIC_BASE 0x1B

static uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ __volatile__("wrmsr" : : "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(msr));
}

static inline uint32_t apic_read(uint32_t reg) {
    return apic_base[reg / 4];
}

static inline void apic_write(uint32_t reg, uint32_t val) {
    apic_base[reg / 4] = val;
}

static inline uint32_t ioapic_read(uint32_t reg) {
    ioapic_base_addr[0] = reg;
    return ioapic_base_addr[4];
}

static inline void ioapic_write(uint32_t reg, uint32_t val) {
    ioapic_base_addr[0] = reg;
    ioapic_base_addr[4] = val;
}

bool apic_detect(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx & (1 << 9)) != 0;
}

void apic_init(void) {
    if (!apic_detect()) {
        kprintf("[APIC] Not supported by CPU, using legacy PIC\n");
        return;
    }
    uint64_t msr = rdmsr(MSR_APIC_BASE);
    uint32_t phys_base = (uint32_t)(msr & 0xFFFFF000);
    msr |= (1ULL << 11);
    wrmsr(MSR_APIC_BASE, msr);
    apic_base = (volatile uint32_t *)(uintptr_t)phys_base;
    paging_map(phys_base, phys_base, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
    apic_write(APIC_DFR, 0xFFFFFFFF);
    apic_write(APIC_LDR, (apic_read(APIC_LDR) & 0x00FFFFFF) | 0x01000000);
    apic_write(APIC_TPR, 0);
    apic_write(APIC_SVR, 0x1FF);
    kprintf("[APIC] Local APIC initialized at 0x%08X (ID: %u)\n", phys_base, apic_get_id());
}

void apic_eoi(void) {
    apic_write(APIC_EOI, 0);
}

uint8_t apic_get_id(void) {
    return (apic_read(APIC_ID) >> 24) & 0xFF;
}

void apic_send_ipi(uint8_t target, uint32_t vector) {
    if (!apic_base) return;
    apic_write(APIC_ICR_HIGH, (uint32_t)target << 24);
    apic_write(APIC_ICR_LOW, vector | APIC_ICR_FIXED);
    uint32_t guard = 100000;
    while ((apic_read(APIC_ICR_LOW) & (1 << 12)) && --guard);
}

void ioapic_init(uint32_t ioapic_phys) {
    ioapic_base_addr = (volatile uint32_t *)(uintptr_t)ioapic_phys;
    paging_map(ioapic_phys, ioapic_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
    uint32_t ver = ioapic_read(IOAPIC_VER);
    uint32_t max_entries = ((ver >> 16) & 0xFF) + 1;
    uint32_t id = (ioapic_read(IOAPIC_ID) >> 24) & 0xF;
    kprintf("[IOAPIC] Initialized at 0x%08X, ID=%u, %u entries\n", ioapic_phys, id, max_entries);
    for (uint32_t i = 0; i < max_entries; i++) {
        ioapic_mask(i);
    }
}

void ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t dest) {
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t low = vector;
    ioapic_write(reg, low);
    uint32_t high = (uint32_t)dest << 24;
    ioapic_write(reg + 1, high);
}

void ioapic_mask(uint8_t irq) {
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t low = ioapic_read(reg);
    ioapic_write(reg, low | (1 << 16));
}

void ioapic_unmask(uint8_t irq) {
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t low = ioapic_read(reg);
    ioapic_write(reg, low & ~(1 << 16));
}
