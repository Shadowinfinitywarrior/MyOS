#include "fbcon.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "framebuffer.h"
#include "font8x8_basic.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static uint32_t cols, rows, cx, cy;
static uint32_t fg = 0xFFFFFF, bg = 0x000000;
#define CW 8
#define CH 8

void fbcon_init(uint32_t w, uint32_t h){
    cols = w / CW;
    rows = h / CH;
    cx = cy = 0;
    fbcon_clear();
    kprintf("[FBCON] %ux%u chars\n", cols, rows);
}

void fbcon_set_color(uint32_t f, uint32_t b){
    fg = f; bg = b;
}

static void draw_char(char c, uint32_t x, uint32_t y) {
    if ((unsigned char)c >= 128) c = '?';
    char *bitmap = font8x8_basic[(int)c];
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (bitmap[r] & (1 << c)) {
                fb_draw_pixel(x + c, y + r, fg);
            } else {
                fb_draw_pixel(x + c, y + r, bg);
            }
        }
    }
}

void fbcon_scroll(void) {
    fb_info_t *fb = fb_get_info();
    uint32_t stride = fb->pitch;
    
    // Move rows 1 to N up by 1 row
    uint32_t row_bytes = stride * CH;
    uint8_t *fb_mem = (uint8_t *)(uintptr_t)fb->virt_addr;
    
    for (uint32_t y = 1; y < rows; y++) {
        memcpy(fb_mem + (y - 1) * row_bytes, fb_mem + y * row_bytes, row_bytes);
    }
    
    // Clear last row
    for (uint32_t y = (rows - 1) * CH; y < rows * CH; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            fb_draw_pixel(x, y, bg);
        }
    }
    cy = rows - 1;
}

void fbcon_putchar(char c) {
    if (c == '\n') {
        cx = 0;
        cy++;
    } else if (c == '\r') {
        cx = 0;
    } else if (c == '\b') {
        if (cx > 0) cx--;
        draw_char(' ', cx * CW, cy * CH);
    } else if (c >= ' ') {
        draw_char(c, cx * CW, cy * CH);
        cx++;
    }
    
    if (cx >= cols) {
        cx = 0;
        cy++;
    }
    
    if (cy >= rows) {
        fbcon_scroll();
    }
}

void fbcon_write(const char *s) {
    while (*s) fbcon_putchar(*s++);
}

void fbcon_clear(void) {
    cx = cy = 0;
    fb_fill(bg);
}
