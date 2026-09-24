#include "framebuffer.h"
#include "../kernel/paging.h"
#include "../include/system.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static fb_info_t fb = {0};

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID     0x0
#define VBE_DISPI_INDEX_XRES   0x1
#define VBE_DISPI_INDEX_YRES   0x2
#define VBE_DISPI_INDEX_BPP    0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK   0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9

#define VBE_DISPI_ID4          0xB0C4
#define VBE_DISPI_ID5          0xB0C5
#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40

static void bga_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t bga_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static uint32_t find_bga_pci_bar0(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t vendor_device = pci_read(bus, slot, 0, 0);
            if (vendor_device == 0xFFFFFFFF) continue;
            uint16_t vendor = vendor_device & 0xFFFF;
            uint16_t device = vendor_device >> 16;
            if ((vendor == 0x1234 && device == 0x1111) ||
                (vendor == 0x1AF4 && device == 0x1050)) {
                uint32_t bar0 = pci_read(bus, slot, 0, 0x10);
                return bar0 & 0xFFFFFFF0;
            }
        }
    }
    return 0;
}

bool fb_detect(void) {
    uint16_t id = bga_read(VBE_DISPI_INDEX_ID);
    return (id >= 0xB0C0 && id <= 0xB0CF);
}

void fb_init(void) {
    if (!fb_detect()) {
        kprintf("[FB] BGA not found.\n");
        return;
    }
    uint32_t fb_phys = find_bga_pci_bar0();
    if (!fb_phys) fb_phys = 0xFD000000;

    uint16_t w = 1024, h = 768, bpp = 32;

    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, w);
    bga_write(VBE_DISPI_INDEX_YRES, h);
    bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, w);
    bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, h);
    bga_write(VBE_DISPI_INDEX_BPP, bpp);
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    fb.phys_addr = fb_phys;

    uint64_t cr0 = read_cr0();
    if (cr0 & 0x80000000ULL) {
        fb.virt_addr = 0xE0000000;
        uint32_t fb_size = (uint32_t)w * (uint32_t)h * (bpp / 8);
        uint32_t fb_size_pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint32_t offset = 0; offset < fb_size_pages * PAGE_SIZE; offset += PAGE_SIZE) {
            paging_map(fb.virt_addr + offset, fb.phys_addr + offset, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
        }
    } else {
        fb.virt_addr = fb.phys_addr;
    }

    fb.width = w; fb.height = h; fb.pitch = w * (bpp / 8); fb.bpp = bpp; fb.depth = bpp;

    kprintf("[FB] Initialized BGA %ux%u\n", fb.width, fb.height);
}

void fb_fill(uint32_t color) {
    if (fb.virt_addr == 0) return;
    uint32_t *dst = (uint32_t *)fb.virt_addr;
    for (uint32_t i = 0; i < fb.width * fb.height; i++) dst[i] = color;
}

void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (fb.virt_addr == 0) return;
    if (x >= fb.width || y >= fb.height) return;
    ((uint32_t *)fb.virt_addr)[y * fb.width + x] = color;
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y) {
    if (fb.virt_addr == 0) return 0;
    if (x >= fb.width || y >= fb.height) return 0;
    return ((uint32_t *)fb.virt_addr)[y * fb.width + x];
}

void fb_wait_vsync(void) {
    uint32_t guard = 2000000;   /* ~0.5s worth at worst; never hang the boot */
    while (inb(0x3DA) & 0x08 && --guard);
    guard = 2000000;
    while (!(inb(0x3DA) & 0x08) && --guard);
}

fb_info_t *fb_get_info(void) { return &fb; }
