#include "pci_ecam.h"
#include "../kernel/paging.h"
#include "../lib/printf.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static uint64_t ecam_base = 0;
static int ecam_available = 0;

int pci_ecam_init(void) {
    acpi_mcfg_t *mcfg = (acpi_mcfg_t *)acpi_find_table("MCFG");
    if (!mcfg) {
        kprintf("[PCIe] ACPI MCFG table not found, using legacy port I/O CF8/CFC\n");
        return -1;
    }

    ecam_base = mcfg->entries[0].base_address;
    uint32_t bus_count = mcfg->entries[0].end_bus - mcfg->entries[0].start_bus + 1;
    uint64_t map_size = (uint64_t)bus_count * 32 * 8 * 4096;

    /* Map first 16MB of ECAM space */
    if (map_size > 16 * 1024 * 1024) map_size = 16 * 1024 * 1024;
    for (uint64_t offset = 0; offset < map_size; offset += 4096) {
        paging_map(ecam_base + offset, ecam_base + offset, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
    }

    ecam_available = 1;
    kprintf("[PCIe] Initialized ECAM MMIO space at 0x%lx (buses %u..%u)\n",
            (unsigned long)ecam_base, mcfg->entries[0].start_bus, mcfg->entries[0].end_bus);
    return 0;
}

uint32_t pci_ecam_read(uint16_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    if (!ecam_available) return 0xFFFFFFFF;
    uint64_t addr = ecam_base |
                    ((uint64_t)bus << 20) |
                    ((uint64_t)slot << 15) |
                    ((uint64_t)func << 12) |
                    (offset & 0xFFF);
    return *(volatile uint32_t *)(uintptr_t)addr;
}

void pci_ecam_write(uint16_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val) {
    if (!ecam_available) return;
    uint64_t addr = ecam_base |
                    ((uint64_t)bus << 20) |
                    ((uint64_t)slot << 15) |
                    ((uint64_t)func << 12) |
                    (offset & 0xFFF);
    *(volatile uint32_t *)(uintptr_t)addr = val;
}
