#ifndef SURFACE_H
#define SURFACE_H

#include "graphics.h"

typedef struct gui_surface {
    int x;
    int y;
    int width;
    int height;
    uint32_t *buffer;
    bool visible;
    bool focused;
    struct gui_surface *next;
    struct gui_surface *prev;
} gui_surface_t;

gui_surface_t *surface_create(int width, int height);
void surface_destroy(gui_surface_t *surface);
void surface_clear(gui_surface_t *surface, gui_color_t color);
void surface_put_pixel(gui_surface_t *surface, int x, int y, gui_color_t color);
gui_color_t surface_get_pixel(gui_surface_t *surface, int x, int y);
void surface_fill_rect(gui_surface_t *surface, int x, int y, int w, int h, gui_color_t color);
void surface_draw_rect(gui_surface_t *surface, int x, int y, int w, int h, gui_color_t color);
void surface_draw_char(gui_surface_t *surface, int x, int y, char c, gui_color_t fg, gui_color_t bg);
void surface_draw_text(gui_surface_t *surface, int x, int y, const char *text, gui_color_t fg, gui_color_t bg);
void surface_draw_vline(gui_surface_t *surface, int x, int y, int h, gui_color_t color);
void surface_draw_hline(gui_surface_t *surface, int x, int y, int w, gui_color_t color);
void surface_blit_to_fb(gui_surface_t *surface);

// Modern UI primitives for surfaces
void surface_fill_rounded_rect(gui_surface_t *surface, int x, int y, int w, int h, int radius, gui_color_t color);
void surface_draw_rounded_rect(gui_surface_t *surface, int x, int y, int w, int h, int radius, gui_color_t color);

static inline uint32_t surface_color_to_uint32(gui_color_t c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}

#endif