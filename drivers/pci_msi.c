#include "pci_msi.h"
#include "pci.h"
#include "../kernel/smp.h"
#include "../lib/printf.h"

int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector) {
    /* Read Status Register at 0x06 */
    uint16_t status = pci_read_cfg16(bus, slot, func, 0x06);
    if (!(status & (1 << 4))) {
        /* Capabilities list not supported */
        return -1;
    }

    uint8_t cap_ptr = pci_read_cfg8(bus, slot, func, 0x34);
    uint8_t msi_offset = 0;
    while (cap_ptr && cap_ptr < 0xFF) {
        uint8_t cap_id = pci_read_cfg8(bus, slot, func, cap_ptr);
        if (cap_id == PCI_CAP_ID_MSI) {
            msi_offset = cap_ptr;
            break;
        }
        cap_ptr = pci_read_cfg8(bus, slot, func, cap_ptr + 1);
    }

    if (!msi_offset) {
        return -1;
    }

    uint16_t msg_ctrl = pci_read_cfg16(bus, slot, func, msi_offset + 2);
    /* Set MSI enable bit (bit 0) */
    msg_ctrl |= (1 << 0);
    /* 1 vector only (bits 4-6 = 0) */
    msg_ctrl &= ~(7 << 4);

    /* Message Address: APIC base (0xFEE00000) | (APIC_ID << 12) */
    uint32_t apic_id = THIS_CPU()->apic_id;
    uint32_t msg_addr = 0xFEE00000 | (apic_id << 12);
    uint32_t msg_data = vector | (0 << 8) | (0 << 14) | (0 << 15);

    pci_write_cfg(bus, slot, func, msi_offset + 4, msg_addr);
    pci_write_cfg(bus, slot, func, msi_offset + 8, msg_data);

    /* Write back message control */
    uint32_t ctrl_val = pci_read_cfg(bus, slot, func, msi_offset);
    ctrl_val = (ctrl_val & 0x0000FFFF) | ((uint32_t)msg_ctrl << 16);
    pci_write_cfg(bus, slot, func, msi_offset, ctrl_val);

    /* Disable legacy INTx in Command register (bit 10) */
    uint16_t cmd = pci_read_cfg16(bus, slot, func, 0x04);
    cmd |= (1 << 10);
    uint32_t cmd_reg = pci_read_cfg(bus, slot, func, 0x04);
    cmd_reg = (cmd_reg & 0xFFFF0000) | cmd;
    pci_write_cfg(bus, slot, func, 0x04, cmd_reg);

    kprintf("[PCI-MSI] Enabled MSI on %02x:%02x.%d -> Vector %u (APIC %u)\n",
            bus, slot, func, vector, apic_id);
    return 0;
}
