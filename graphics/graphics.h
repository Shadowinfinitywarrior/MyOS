#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "../include/system.h"
#include "../drivers/framebuffer.h"

#define GUI_FONT_WIDTH  8
#define GUI_FONT_HEIGHT 8

typedef struct gui_rect {
    int x;
    int y;
    int width;
    int height;
} gui_rect_t;

typedef struct gui_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gui_color_t;

typedef struct gui_point {
    int x;
    int y;
} gui_point_t;

#define GUI_COLOR(r, g, b) (gui_color_t){(r), (g), (b), 255}
#define GUI_COLOR_RGBA(r, g, b, a) (gui_color_t){(r), (g), (b), (a)}
#define GUI_COLOR_RGB(rgb) (gui_color_t){((rgb) >> 16) & 0xFF, ((rgb) >> 8) & 0xFF, (rgb) & 0xFF, 255}

// Professional Windows 11/macOS inspired color palette
#define COLOR_BLACK   GUI_COLOR(0x00, 0x00, 0x00)
#define COLOR_WHITE   GUI_COLOR(0xFF, 0xFF, 0xFF)
#define COLOR_RED     GUI_COLOR(0xFF, 0x45, 0x3A)
#define COLOR_GREEN   GUI_COLOR(0x00, 0x89, 0x59)
#define COLOR_BLUE    GUI_COLOR(0x00, 0x67, 0xFF)
#define COLOR_GRAY    GUI_COLOR(0x80, 0x80, 0x80)
#define COLOR_DARK_GRAY    GUI_COLOR(0x1A, 0x1A, 0x1A)
#define COLOR_MID_GRAY     GUI_COLOR(0x4A, 0x4A, 0x4A)
#define COLOR_LIGHT_GRAY   GUI_COLOR(0xD4, 0xD4, 0xD4)
#define COLOR_SPACE_BLUE   GUI_COLOR(0x0D, 0x1B, 0x2A)
#define COLOR_SPACE_DARK   GUI_COLOR(0x05, 0x0A, 0x14)
#define COLOR_SPACE_ACCENT GUI_COLOR(0x00, 0xAA, 0xFF)
#define COLOR_MYOS_BLUE    GUI_COLOR(0x20, 0x64, 0xB3)
#define COLOR_MYOS_DARK    GUI_COLOR(0x0C, 0x1A, 0x2E)
#define COLOR_MYOS_ACCENT  GUI_COLOR(0x00, 0x7A, 0xFF)

// Modern UI colors
#define COLOR_WINDOW_BG       GUI_COLOR(0xF3, 0xF3, 0xF3)
#define COLOR_WINDOW_ACTIVE  GUI_COLOR(0xFF, 0xFF, 0xFF)
#define COLOR_WINDOW_INACTIVE GUI_COLOR(0xE8, 0xE8, 0xE8)
#define COLOR_TITLEBAR       GUI_COLOR(0x20, 0x20, 0x20)
#define COLOR_TITLEBAR_INACTIVE GUI_COLOR(0x40, 0x40, 0x40)
#define COLOR_BUTTON         GUI_COLOR(0x00, 0x67, 0xFF)
#define COLOR_BUTTON_HOVER   GUI_COLOR(0x00, 0x50, 0xDD)
#define COLOR_BUTTON_TEXT    GUI_COLOR(0xFF, 0xFF, 0xFF)
#define COLOR_BORDER         GUI_COLOR(0xE0, 0xE0, 0xE0)
#define COLOR_SHADOW         GUI_COLOR_RGBA(0x00, 0x00, 0x00, 0x20)
#define COLOR_GLASS          GUI_COLOR_RGBA(0xF0, 0xF0, 0xF0, 0xC0)

void gui_graphics_init(void);
void gui_clear(gui_color_t color);
void gui_put_pixel(int x, int y, gui_color_t color);
gui_color_t gui_get_pixel(int x, int y);
void gui_fill_rect(int x, int y, int w, int h, gui_color_t color);
void gui_draw_rect(int x, int y, int w, int h, gui_color_t color);
void gui_draw_line(int x1, int y1, int x2, int y2, gui_color_t color);
void gui_draw_hline(int x, int y, int w, gui_color_t color);
void gui_draw_vline(int x, int y, int h, gui_color_t color);
void gui_draw_char(int x, int y, char c, gui_color_t fg, gui_color_t bg);
void gui_draw_text(int x, int y, const char *text, gui_color_t fg, gui_color_t bg);
int gui_text_width(const char *text);
int gui_text_height(void);
void gui_blit(int dst_x, int dst_y, int src_x, int src_y, int w, int h, uint32_t *src_buffer);

// Modern UI primitives
void gui_draw_rounded_rect(int x, int y, int w, int h, int radius, gui_color_t color);
void gui_fill_rounded_rect(int x, int y, int w, int h, int radius, gui_color_t color);
void gui_draw_shadow(int x, int y, int w, int h, int blur, gui_color_t color);
void gui_draw_glass_effect(int x, int y, int w, int h, gui_color_t bg_color, int alpha);

fb_info_t *gui_get_fb_info(void);

#endif