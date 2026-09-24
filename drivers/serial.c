#include "serial.h"
#include "../include/types.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define COM1 0x3F8
#define DEBUG_PORT 0xE9

void serial_init(int com) {
    (void)com;
    
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x01);    // Set divisor to 1 (lo byte) 115200 baud - QEMU default
    outb(COM1 + 1, 0x00);    //                  (hi byte) 115200 baud - QEMU default
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x03);    // IRQs disabled, RTS/DSR set
}

void serial_write(char c) {
    /* Write to QEMU debug port (0xE9) for immediate output */
    outb(DEBUG_PORT, c);

    /* Also write to COM1, but never hang on a broken/absent UART: bail after
     * a bounded number of LSR probes rather than spinning forever. */
    uint32_t tries = 100000;
    while ((inb(COM1 + 5) & 0x20) == 0 && --tries);
    outb(COM1, c);
}

int serial_read(void) {
    if (inb(COM1 + 5) & 0x01) {
        return inb(COM1);
    }
    return -1;
}

bool serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}