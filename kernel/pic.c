#include "pic.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define PIC1      0x20
#define PIC2      0xA0
#define PIC1_CMD  PIC1
#define PIC2_CMD  PIC2
#define PIC1_DATA (PIC1 + 1)
#define PIC2_DATA (PIC2 + 1)

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

void pic_init(void) {
    /* ICW1: initialization sequence */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: vector offsets (master -> 0x20, slave -> 0x28) */
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    /* ICW3: cascade setup (slave on master IRQ2) */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Mask everything, then unmask all IRQs */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    for (int irq = 0; irq < 16; irq++)
        pic_clear_mask((uint8_t)irq);

    kprintf("[PIC] Remapped to vectors 0x20/0x28\n");
}

void pic_clear_mask(uint8_t irq) {
    if (irq > 15) return;
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    outb(port, inb(port) & ~(1 << (irq & 7)));
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}