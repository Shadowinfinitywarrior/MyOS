#include "irq.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
void irq_init(void) { kprintf("[IRQ] Initialized\n"); }
void irq_install_handler(int irq, void (*handler)(void)) { (void)irq; (void)handler; }
void irq_uninstall_handler(int irq) { (void)irq; }
