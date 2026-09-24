#include "usb.h"
#include "pci.h"
#include "xhci.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define USB_XHCI_VENDOR 0x1B36   /* RedHat */
#define USB_XHCI_DEVICE 0x000D   /* qemu-xhci */

/* USB subsystem entry point, called from gui/desktop.c after the PS/2 mouse.
 * If no xHCI controller is present we print a [XHCI] notice and return -1 so
 * the PS/2 input path keeps working untouched. */
int usb_init(void) {
    pci_dev_info_t info;
    if (!pci_find_device(USB_XHCI_VENDOR, USB_XHCI_DEVICE, &info)) {
        kprintf("[XHCI] no controller found\n");
        return -1;
    }

    kprintf("[XHCI] found 0x%X:0x%X at %u:%u.%u IRQ line %u\n",
            info.vendor, info.device,
            info.loc.bus, info.loc.slot, info.loc.func, info.irq_line);
    kprintf("[XHCI] BAR0 (MMIO base) = 0x%X\n", info.bar0);

    xhci_init(info.bar0);
    return xhci_enum_devices();
}

int usb_probe(usb_dev_t *d) {
    (void)d;
    return 0;
}