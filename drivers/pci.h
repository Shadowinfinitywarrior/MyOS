#ifndef PCI_H
#define PCI_H
#include "../include/system.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
} pci_loc_t;

typedef struct {
    pci_loc_t loc;
    uint16_t  vendor;
    uint16_t  device;
    uint32_t  bar0;       /* decoded base (32-bit BAR low / low half of 64-bit BAR), flags masked */
    uint8_t   irq_line;   /* legacy INTx# routing, config offset 0x3C */
} pci_dev_info_t;

uint32_t pci_read_cfg(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_cfg8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_cfg16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write_cfg(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

/* Scan all 256 config buses for vendor:device. Fills info (bars, irq line)
 * on hit and returns 1; returns 0 if the device is not present. */
int pci_find_device(uint16_t vendor, uint16_t device, pci_dev_info_t *info);

void pci_init(void);

#endif