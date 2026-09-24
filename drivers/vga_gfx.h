#ifndef VGA_GFX_H
#define VGA_GFX_H

#include "../include/types.h"

#define GFX_WIDTH   320
#define GFX_HEIGHT  200
#define GFX_BPP     8       /* 8 bits per pixel = 256 colors */
#define GFX_VRAM    0xA0000

/* Standard VGA 256-color palette indices */
#define GFX_BLACK       0
#define GFX_BLUE        1
#define GFX_GREEN       2
#define GFX_CYAN        3
#define GFX_RED         4
#define GFX_MAGENTA     5
#define GFX_BROWN       6
#define GFX_WHITE       7
#define GFX_GREY        8
#define GFX_LIGHT_BLUE  9
#define GFX_LIGHT_GREEN 10
#define GFX_YELLOW      14

void gfx_init(void);
void gfx_exit(void);
void gfx_clear(uint8_t color);
void gfx_putpixel(int x, int y, uint8_t color);
uint8_t gfx_getpixel(int x, int y);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint8_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint8_t color);
void gfx_draw_circle(int cx, int cy, int r, uint8_t color);
void gfx_fill_circle(int cx, int cy, int r, uint8_t color);
void gfx_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg);
void gfx_draw_string(int x, int y, const char *str, uint8_t fg);
void gfx_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void gfx_vsync(void);

#endif

