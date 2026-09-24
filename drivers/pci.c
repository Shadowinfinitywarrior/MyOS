#include "pci.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static uint32_t pci_make_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return 0x80000000u                       /* config enable */
         | ((uint32_t)bus << 16)
         | ((uint32_t)slot << 11)
         | ((uint32_t)func << 8)
         | (offset & 0xFC);
}

uint32_t pci_read_cfg(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_make_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint8_t pci_read_cfg8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read_cfg(bus, slot, func, offset & 0xFC);
    return (uint8_t)(v >> ((offset & 3) * 8));
}

uint16_t pci_read_cfg16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read_cfg(bus, slot, func, offset & 0xFE);
    return (uint16_t)(v >> ((offset & 2) * 8));
}

void pci_write_cfg(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_make_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

int pci_find_device(uint16_t vendor, uint16_t device, pci_dev_info_t *info) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t vd = pci_read_cfg(bus, slot, func, 0x00);
                if ((vd & 0xFFFF) == 0xFFFF) {
                    if (func == 0) break;   /* empty slot, skip remaining funcs */
                    continue;
                }
                if ((vd & 0xFFFF) != vendor || (vd >> 16) != device)
                    continue;

                if (info) {
                    info->loc.bus  = (uint8_t)bus;
                    info->loc.slot = (uint8_t)slot;
                    info->loc.func = (uint8_t)func;
                    info->vendor   = (uint16_t)(vd & 0xFFFF);
                    info->device   = (uint16_t)(vd >> 16);

                    /* BAR0 decode: i/o or mmio flag in bit0, 64-bit marker in bit2.
                     * A 64-bit BAR spans dwords 0x10/0x14; on this 32-bit (no-paging)
                     * kernel only the low half can be a usable address. */
                    uint32_t bar0_lo = pci_read_cfg(bus, slot, func, 0x10);
                    uint32_t bar0_hi = pci_read_cfg(bus, slot, func, 0x14);
                    if (bar0_lo & 0x04) {
                        if (bar0_hi) {
                            /* high half non-zero -> base above 4GB, not reachable */
                            info->bar0 = 0;
                        } else {
                            info->bar0 = bar0_lo & 0xFFFFFFF0;
                        }
                    } else {
                        info->bar0 = bar0_lo & 0xFFFFFFF0;
                    }

                    info->irq_line = pci_read_cfg8(bus, slot, func, 0x3C);
                }
                return 1;
            }
        }
    }
    return 0;
}

void pci_init(void) {}