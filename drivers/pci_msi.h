#ifndef PCI_MSI_H
#define PCI_MSI_H

#include "../include/types.h"
#include "pci.h"

#define PCI_CAP_ID_MSI 0x05

int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector);

#endif
