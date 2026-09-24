#ifndef XHCI_H
#define XHCI_H
#include "../include/system.h"
void xhci_init(uint32_t bar);
int xhci_enum_devices(void);

/* Stage B hook: event handler for the xHCI controller.
 * Intended vector = 32 + INTx line read from PCI config 0x3C (e.g. 32+11=43).
 * Stage A only reports the vector; Stage B will register it and unmask it via
 *   isr_register_handler(32 + irq_line, xhci_isr);
 *   pic_clear_mask(irq_line);
 */
void xhci_isr(registers_t *regs);
#endif