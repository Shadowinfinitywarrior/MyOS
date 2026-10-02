#include "framebuffer.h"
#include "../kernel/paging.h"
#include "../kernel/pmm.h"
#include "../include/system.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static fb_info_t fb = {0};

/* ---- Back buffer + damage tracking -------------------------------------- */

static uint32_t *bb;                 /* cached software render target      */
static int bb_stride;                /* pixels per row (== fb.width)       */
static bool bb_ready;

/* Damage is kept as a union of the requested rects. A GUI frame touches many
 * separate pieces (window, shadow, taskbar, cursor) but the LFB copy is much
 * cheaper as a handful of large spans than as dozens of small ones, so rects
 * are accumulated and flushed together rather than immediately. */
#define FB_DAMAGE_MAX 64
typedef struct { int x, y, w, h; } fb_rect_t;
static fb_rect_t damage[FB_DAMAGE_MAX];
static int damage_count;

/* LFB accessors: the back buffer when present, the LFB directly otherwise. */
static inline uint32_t *fb_dst(void) { return bb ? bb : (uint32_t *)(uintptr_t)fb.virt_addr; }

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
        fb.virt_addr = fb_phys;
    }

    fb.width = w; fb.height = h; fb.pitch = w * (bpp / 8); fb.bpp = bpp; fb.depth = bpp;
    fb.refresh_rate = 60;
    fb.vsync_enabled = false;

    /* Back buffer: one 32-bit pixel per LFB pixel. Allocated through the PMM
     * (not the heap) because it is a contiguous, page-aligned, long-lived
     * allocation that must survive heap growth. */
    uint32_t pages = (uint32_t)(((uint64_t)w * h * 4 + PAGE_SIZE - 1) / PAGE_SIZE);
    uint64_t p = pmm_alloc_contiguous(pages);
    if (p) {
        /* Map the physical run into a fixed high kernel virtual slot. A second
         * mapping is harmless: the LFB mapping above uses 0xE0000000, so the
         * back buffer takes 0xE4000000. */
        uint64_t base = 0xE4000000ULL;
        for (uint32_t i = 0; i < pages; i++) {
            paging_map(base + (uint64_t)i * PAGE_SIZE, p + (uint64_t)i * PAGE_SIZE,
                       PAGE_PRESENT | PAGE_WRITE);
        }
        bb = (uint32_t *)(uintptr_t)base;
        bb_stride = (int)w;
        bb_ready = true;
    } else {
        bb = NULL;
        bb_stride = (int)w;
    }

    kprintf("[FB] Initialized BGA %ux%u pitch=%u phys=0x%X%s\n",
            fb.width, fb.height, fb.pitch, fb.phys_addr,
            bb_ready ? " (double-buffered)" : " (direct)");

    if (bb) {
        fb_fill(0x000000);
        fb_flush_all();
    }
}

uint32_t *fb_get_backbuffer(void) { return bb_ready ? bb : NULL; }
int fb_get_stride(void) { return bb_stride; }
uint32_t fb_damage_count(void) { return (uint32_t)damage_count; }

void fb_clear_damage(void) { damage_count = 0; }
bool fb_has_damage(void) { return damage_count > 0; }

void fb_add_damage(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = (int)fb.width - x;
    if (y + h > (int)fb.height) h = (int)fb.height - y;
    if (w <= 0 || h <= 0) return;

    /* Coalesce into the first rect that already contains this one; otherwise
     * append. The list is small because frames touch a bounded number of
     * surfaces, and fb_flush() merges whatever accumulated. */
    for (int i = 0; i < damage_count; i++) {
        fb_rect_t *r = &damage[i];
        if (x >= r->x && y >= r->y && x + w <= r->x + r->w && y + h <= r->y + r->h)
            return;
    }
    for (int i = 0; i < damage_count; i++) {
        fb_rect_t *r = &damage[i];
        if (x < r->x + r->w && r->x < x + w && y < r->y + r->h && r->y < y + h) {
            int nx = r->x < x ? r->x : x;
            int ny = r->y < y ? r->y : y;
            int ex = (r->x + r->w) > (x + w) ? (r->x + r->w) : (x + w);
            int ey = (r->y + r->h) > (y + h) ? (r->y + r->h) : (y + h);
            r->x = nx; r->y = ny; r->w = ex - nx; r->h = ey - ny;
            return;
        }
    }
    if (damage_count < FB_DAMAGE_MAX) {
        damage[damage_count].x = x;
        damage[damage_count].y = y;
        damage[damage_count].w = w;
        damage[damage_count].h = h;
        damage_count++;
    } else {
        /* Fall back to repainting everything rather than dropping damage. */
        damage[0].x = 0; damage[0].y = 0;
        damage[0].w = (int)fb.width; damage[0].h = (int)fb.height;
        damage_count = 1;
    }
}

static void fb_copy_to_lfb(int x, int y, int w, int h) {
    if (!bb || fb.virt_addr == 0) return;
    uint32_t *dst = (uint32_t *)(uintptr_t)fb.virt_addr;
    uint32_t lfb_stride = fb.pitch / 4;
    for (int row = 0; row < h; row++) {
        const uint32_t *s = bb + (uint32_t)((y + row) * bb_stride + x);
        uint32_t *d = dst + (uint32_t)((y + row) * lfb_stride + x);
        for (int col = 0; col < w; col++) d[col] = s[col];
    }
}

void fb_flush(void) {
    if (!bb || fb.virt_addr == 0) { fb_clear_damage(); return; }
    for (int i = 0; i < damage_count; i++) {
        fb_copy_to_lfb(damage[i].x, damage[i].y, damage[i].w, damage[i].h);
    }
    fb_clear_damage();
    fb.frame_count++;
}

void fb_flush_all(void) {
    if (!bb || fb.virt_addr == 0) return;
    fb_copy_to_lfb(0, 0, (int)fb.width, (int)fb.height);
    fb_clear_damage();
    fb.frame_count++;
}

void fb_fill(uint32_t color) {
    if (fb.virt_addr == 0) return;
    uint32_t *dst = fb_dst();
    int stride = bb ? bb_stride : (int)(fb.pitch / 4);
    for (uint32_t y = 0; y < fb.height; y++) {
        uint32_t *row = dst + y * (uint32_t)stride;
        for (uint32_t x = 0; x < fb.width; x++) row[x] = color;
    }
    fb_add_damage(0, 0, (int)fb.width, (int)fb.height);
}

void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (fb.virt_addr == 0) return;
    if (x >= fb.width || y >= fb.height) return;
    uint32_t *dst = fb_dst();
    int stride = bb ? bb_stride : (int)(fb.pitch / 4);
    dst[y * (uint32_t)stride + x] = color;
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y) {
    if (fb.virt_addr == 0) return 0;
    if (x >= fb.width || y >= fb.height) return 0;
    uint32_t *dst = fb_dst();
    int stride = bb ? bb_stride : (int)(fb.pitch / 4);
    return dst[y * (uint32_t)stride + x];
}

void fb_wait_vsync(void) {
    uint32_t guard = 2000000;   /* ~0.5s worth at worst; never hang the boot */
    while (inb(0x3DA) & 0x08 && --guard);
    guard = 2000000;
    while (!(inb(0x3DA) & 0x08) && --guard);
}

fb_info_t *fb_get_info(void) { return &fb; }
