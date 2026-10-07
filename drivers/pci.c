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

#define MAX_PCI_DEVICES 64
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_dev_count = 0;

int pci_device_count(void) {
    return pci_dev_count;
}

const pci_device_t *pci_get_device(int idx) {
    if (idx < 0 || idx >= pci_dev_count) return NULL;
    return &pci_devices[idx];
}

int pci_find_by_class(uint8_t class_code, uint8_t subclass, pci_device_t *out) {
    for (int i = 0; i < pci_dev_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass) {
            if (out) *out = pci_devices[i];
            return 1;
        }
    }
    return 0;
}

int pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_read_cfg16(bus, slot, func, 0x04);
    cmd |= (1 << 2) | (1 << 0); /* Bus Master Enable + I/O Space Enable */
    pci_write_cfg(bus, slot, func, 0x04, (uint32_t)cmd);
    return 0;
}

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar_idx) {
    if (bar_idx < 0 || bar_idx > 5) return 0;
    uint32_t raw = pci_read_cfg(bus, slot, func, 0x10 + (bar_idx * 4));
    if (raw & 1) {
        return raw & ~0x3; /* I/O Port */
    } else {
        return raw & ~0xF; /* MMIO Base */
    }
}

void pci_init(void) {
    pci_dev_count = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint8_t header_type = pci_read_cfg8((uint8_t)bus, (uint8_t)slot, 0, 0x0E);
            int max_func = (header_type & 0x80) ? 8 : 1;

            for (int func = 0; func < max_func; func++) {
                uint32_t vd = pci_read_cfg((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                uint16_t vendor = (uint16_t)(vd & 0xFFFF);
                uint16_t device = (uint16_t)(vd >> 16);

                if (vendor == 0xFFFF || vendor == 0x0000) {
                    continue;
                }

                if (pci_dev_count < MAX_PCI_DEVICES) {
                    pci_device_t *dev = &pci_devices[pci_dev_count++];
                    dev->loc.bus = (uint8_t)bus;
                    dev->loc.slot = (uint8_t)slot;
                    dev->loc.func = (uint8_t)func;
                    dev->vendor = vendor;
                    dev->device = device;
                    dev->header_type = header_type;

                    uint32_t class_rev = pci_read_cfg((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x08);
                    dev->revision = (uint8_t)(class_rev & 0xFF);
                    dev->progif = (uint8_t)((class_rev >> 8) & 0xFF);
                    dev->subclass = (uint8_t)((class_rev >> 16) & 0xFF);
                    dev->class_code = (uint8_t)((class_rev >> 24) & 0xFF);

                    dev->irq_line = pci_read_cfg8((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x3C);

                    for (int b = 0; b < 6; b++) {
                        uint32_t raw_bar = pci_read_cfg((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x10 + b * 4);
                        if (raw_bar & 1) {
                            dev->bar_is_io[b] = 1;
                            dev->bars[b] = raw_bar & ~0x3;
                        } else {
                            dev->bar_is_io[b] = 0;
                            dev->bars[b] = raw_bar & ~0xF;
                        }
                    }
                }
            }
        }
    }
}