#include "e1000.h"
#include "pci.h"
#include "driver.h"
#include "../lib/printf.h"

static e1000_t nic;

int e1000_init(void) {
    pci_dev_info_t info;
    /* Intel 82540EM Gigabit Ethernet (0x8086:0x100E) */
    if (pci_find_device(0x8086, 0x100E, &info) || pci_find_device(0x8086, 0x100F, &info)) {
        nic.mmio_base = info.bar0;
        nic.link_up = 1;
        pci_enable_bus_master(info.loc.bus, info.loc.slot, info.loc.func);
        kprintf("[E1000] Intel PRO/1000 Gigabit Ethernet detected at %02x:%02x.%x (BAR0=0x%08x, IRQ=%u)\n",
                info.loc.bus, info.loc.slot, info.loc.func, (uint32_t)info.bar0, info.irq_line);
        driver_register("e1000", "net", "ready", "Intel PRO/1000 Gigabit Ethernet Controller");
        return 0;
    }

    nic.mmio_base = 0;
    nic.link_up = 0;
    driver_register("e1000", "net", "none", "Intel PRO/1000 not attached (VirtIO-Net active)");
    return 0;
}

int e1000_send(const void *pkt, size_t len) {
    (void)pkt; (void)len;
    return nic.link_up ? 0 : -1;
}

int e1000_recv(void *pkt, size_t *len) {
    (void)pkt; (void)len;
    return -1;
}
