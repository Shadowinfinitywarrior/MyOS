#include "ne2k.h"
#include "../include/system.h"
#include "../kernel/paging.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static ne2k_dev_t ne2k;

static void ne2k_outb(uint16_t port, uint8_t val) { outb(port, val); }
static uint8_t ne2k_inb(uint16_t port) { return inb(port); }

bool ne2k_detect(void) {
    uint8_t val = ne2k_inb(NE2K_IO_BASE + NE2K_RESET);
    ne2k_outb(NE2K_IO_BASE + NE2K_RESET, 0x01);
    for (volatile int i = 0; i < 1000; i++);
    uint8_t val2 = ne2k_inb(NE2K_IO_BASE + NE2K_RESET);
    return (val != val2);
}

int ne2k_init(uint16_t io_base, uint8_t irq) {
    ne2k.io_base = io_base;
    ne2k.irq = irq;
    paging_map(NE2K_MEM_BASE, NE2K_MEM_BASE, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
    ne2k.shared_ram = (uint8_t *)NE2K_MEM_BASE;
    ne2k_outb(io_base + NE2K_RESET, 0x01);
    for (volatile int i = 0; i < 1000; i++);
    ne2k_outb(io_base + NE2K_INTR_MASK, 0xFF);
    ne2k_outb(io_base + NE2K_INTR_MASK, 0x00);
    ne2k_outb(io_base + NE2K_CMD_REG, NE2K_CMD_STOP);
    ne2k_outb(io_base + NE2K_CMD_REG, NE2K_CMD_START);
    memset(ne2k.mac, 0, 6);
    ne2k.mac[0] = 0xDE;
    ne2k.mac[5] = 0xAD;
    kprintf("[NE2K] Initialized at 0x%03X IRQ %u\n", io_base, irq);
    return 0;
}

int ne2k_send(ne2k_dev_t *dev, uint8_t *buf, uint16_t len) {
    if (!dev || !buf) return -1;
    uint16_t page = dev->page_current;
    uint16_t offset = page * 256;
    memcpy(dev->shared_ram + offset, buf, len);
    ne2k_outb(dev->io_base + NE2K_DATA_PTR, offset & 0xFF);
    ne2k_outb(dev->io_base + NE2K_DATA_PTR + 1, (offset >> 8) & 0xFF);
    ne2k_outb(dev->io_base + NE2K_BCNT_LO, len & 0xFF);
    ne2k_outb(dev->io_base + NE2K_BCNT_HI, (len >> 8) & 0xFF);
    ne2k_outb(dev->io_base + NE2K_CMD_REG, NE2K_CMD_READ);
    return len;
}

int ne2k_recv(ne2k_dev_t *dev, uint8_t *buf, uint16_t max_len) {
    if (!dev || !buf) return -1;
    uint16_t rc = ne2k_inb(dev->io_base + NE2K_INTR_MASK);
    if (!(rc & 0x01)) return 0;
    uint16_t page = dev->page_current;
    uint16_t offset = page * 256;
    uint16_t len = * (uint16_t *)(dev->shared_ram + offset);
    if (len > max_len) len = max_len;
    memcpy(buf, dev->shared_ram + offset + 2, len);
    return len;
}

void ne2k_irq_handler(void) {
    uint8_t status = ne2k_inb(ne2k.io_base + NE2K_INTR_MASK);
    if (status & 0x01) {
    }
}
