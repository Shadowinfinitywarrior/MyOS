#ifndef PCI_ECAM_H
#define PCI_ECAM_H

#include "../include/types.h"
#include "../kernel/acpi.h"

typedef struct {
    uint64_t base_address;
    uint16_t segment_group_count;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} PACKED acpi_mcfg_entry_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t          reserved;
    acpi_mcfg_entry_t entries[];
} PACKED acpi_mcfg_t;

int pci_ecam_init(void);
uint32_t pci_ecam_read(uint16_t bus, uint8_t slot, uint8_t func, uint16_t offset);
void pci_ecam_write(uint16_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val);

#endif
