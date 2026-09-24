#include "smp.h"
#include "acpi.h"
#include "apic.h"
#include "pmm.h"
#include "paging.h"
#include "timer.h"
#include "../include/system.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define TRAMPOLINE_PHYS     0x8000
#define TRAMPOLINE_SIZE     0x1000
#define AP_STACK_SIZE       8192

#define OFF_STACK_PTR   0x38
#define OFF_CR3         0x3C
#define OFF_IDT_PTR     0x40
#define OFF_ENTRY       0x48
#define OFF_READY_FLAG  0x4C

static cpu_info_t cpus[MAX_CPUS];
static int cpu_count = 0;
static volatile uint32_t ap_ready = 0;
static uint8_t ap_stacks[MAX_CPUS][AP_STACK_SIZE] ALIGNED(16);

extern uint8_t TRAMPOLINE_START[];
extern uint8_t TRAMPOLINE_END[];

void ap_main(uint32_t apic_id) {
    kprintf("[SMP] AP %u (APIC ID %u) is alive!\n", cpu_count, apic_id);
    sti();
    while (1) { hlt(); }
}

void smp_init(void) {
    memset(cpus, 0, sizeof(cpus));
    cpus[0].apic_id = apic_get_id();
    cpus[0].is_bsp = true;
    cpus[0].active = true;
    cpu_count = 1;

    int acpi_cpus = acpi_get_num_cpus();
    kprintf("[SMP] ACPI reports %d CPU(s)\n", acpi_cpus);
    if (acpi_cpus <= 1) { kprintf("[SMP] Uniprocessor system\n"); return; }
    if (!apic_detect()) { kprintf("[SMP] No APIC\n"); return; }

    uint32_t tramp_size = (uint32_t)(TRAMPOLINE_END - TRAMPOLINE_START);
    if (tramp_size > TRAMPOLINE_SIZE) tramp_size = TRAMPOLINE_SIZE;
    memcpy((void *)TRAMPOLINE_PHYS, TRAMPOLINE_START, tramp_size);
    paging_map(TRAMPOLINE_PHYS, TRAMPOLINE_PHYS, PAGE_PRESENT | PAGE_WRITE);

    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (!madt) { kprintf("[SMP] No MADT\n"); return; }

    uint8_t *ptr = madt->entries;
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    int ap_index = 1;

    while (ptr < end && ap_index < MAX_CPUS) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];
        if (type == MADT_LOCAL_APIC) {
            madt_local_apic_t *lapic = (madt_local_apic_t *)ptr;
            if ((lapic->flags & 1) && lapic->apic_id != cpus[0].apic_id) {
                uint8_t *tramp = (uint8_t *)TRAMPOLINE_PHYS;
                uint32_t stack_top = (uint32_t)&ap_stacks[ap_index][AP_STACK_SIZE];
                *(uint32_t *)(tramp + OFF_STACK_PTR) = stack_top;
                *(uint32_t *)(tramp + OFF_CR3) = read_cr3();
                uint16_t idt_ptr[3]; __asm__ __volatile__("sidt (%0)" : : "r"(idt_ptr));
                memcpy(tramp + OFF_IDT_PTR, idt_ptr, 6);
                *(uint32_t *)(tramp + OFF_ENTRY) = (uint32_t)ap_main;
                ap_ready = 0;
                *(uint32_t *)(tramp + OFF_READY_FLAG) = (uint32_t)&ap_ready;

                apic_send_ipi(lapic->apic_id, 0x00004500);
                timer_sleep(10);
                uint32_t vector = TRAMPOLINE_PHYS / PAGE_SIZE;
                apic_send_ipi(lapic->apic_id, 0x00004600 | vector);
                timer_sleep(1);
                apic_send_ipi(lapic->apic_id, 0x00004600 | vector);
                timer_sleep(1);

                int timeout = 1000;
                while (!ap_ready && timeout > 0) { timer_sleep(1); timeout--; }

                if (ap_ready) {
                    cpus[ap_index].apic_id = lapic->apic_id;
                    cpus[ap_index].active = true;
                    cpus[ap_index].is_bsp = false;
                    cpu_count++;
                    kprintf("[SMP] AP %u started\n", lapic->apic_id);
                } else {
                    kprintf("[SMP] AP %u failed\n", lapic->apic_id);
                }
                ap_index++;
            }
        }
        ptr += length;
    }
    kprintf("[SMP] Total active CPUs: %d\n", cpu_count);
}

int smp_get_cpu_count(void) { return cpu_count; }
uint8_t smp_get_current_id(void) { return apic_get_id(); }
cpu_info_t *smp_get_cpu(int index) {
    if (index >= 0 && index < cpu_count) return &cpus[index];
    return NULL;
}
