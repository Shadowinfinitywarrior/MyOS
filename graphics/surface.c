#include "surface.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "graphics.h"
#include "../drivers/framebuffer.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

// RGBA color macro for alpha channel
#define GUI_COLOR_RGBA(r, g, b, a) (gui_color_t){(r), (g), (b), (a)}

gui_surface_t *surface_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    
    gui_surface_t *surface = (gui_surface_t *)kmalloc(sizeof(gui_surface_t));
    if (!surface) return NULL;
    
    surface->buffer = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!surface->buffer) {
        kfree(surface);
        return NULL;
    }
    
    surface->x = 0;
    surface->y = 0;
    surface->width = width;
    surface->height = height;
    surface->visible = true;
    surface->focused = false;
    surface->next = NULL;
    surface->prev = NULL;
    
    return surface;
}

void surface_destroy(gui_surface_t *surface) {
    if (!surface) return;
    if (surface->buffer) kfree(surface->buffer);
    kfree(surface);
}

void surface_clear(gui_surface_t *surface, gui_color_t color) {
    if (!surface || !surface->buffer) return;
    uint32_t color32 = surface_color_to_uint32(color);
    for (int i = 0; i < surface->width * surface->height; i++) {
        surface->buffer[i] = color32;
    }
}

void surface_put_pixel(gui_surface_t *surface, int x, int y, gui_color_t color) {
    if (!surface || !surface->buffer) return;
    if (x < 0 || x >= surface->width || y < 0 || y >= surface->height) return;
    surface->buffer[y * surface->width + x] = surface_color_to_uint32(color);
}

gui_color_t surface_get_pixel(gui_surface_t *surface, int x, int y) {
    gui_color_t c = {0, 0, 0, 0};
    if (!surface || !surface->buffer) return c;
    if (x < 0 || x >= surface->width || y < 0 || y >= surface->height) return c;
    uint32_t c32 = surface->buffer[y * surface->width + x];
    return (gui_color_t){(c32 >> 16) & 0xFF, (c32 >> 8) & 0xFF, c32 & 0xFF, 255};
}

void surface_fill_rect(gui_surface_t *surface, int x, int y, int w, int h, gui_color_t color) {
    if (!surface || !surface->buffer || w <= 0 || h <= 0) return;
    
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w > surface->width) ? surface->width : (x + w);
    int y2 = (y + h > surface->height) ? surface->height : (y + h);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    for (int yy = y1; yy < y2; yy++) {
        uint32_t *row = surface->buffer + yy * surface->width + x1;
        for (int xx = x1; xx < x2; xx++) {
            row[xx - x1] = color32;
        }
    }
}

void surface_draw_rect(gui_surface_t *surface, int x, int y, int w, int h, gui_color_t color) {
    if (!surface || !surface->buffer || w <= 0 || h <= 0) return;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w > surface->width) ? surface->width : (x + w);
    int y2 = (y + h > surface->height) ? surface->height : (y + h);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    // Top and bottom
    for (int xx = x1; xx < x2; xx++) {
        if (y < surface->height) surface->buffer[y * surface->width + xx] = color32;
        if (y + h - 1 < surface->height) surface->buffer[(y + h - 1) * surface->width + xx] = color32;
    }
    // Left and right
    for (int yy = y1; yy < y2; yy++) {
        if (x < surface->width) surface->buffer[yy * surface->width + x] = color32;
        if (x + w - 1 < surface->width) surface->buffer[yy * surface->width + x + w - 1] = color32;
    }
}

void surface_draw_char(gui_surface_t *surface, int x, int y, char c, gui_color_t fg, gui_color_t bg) {
    if (!surface || !surface->buffer) return;
    if ((unsigned char)c >= 128) c = '?';
    
    extern char font8x8_basic[128][8];
    char *bitmap = font8x8_basic[(int)c];
    
    for (int r = 0; r < 8; r++) {
        int yy = y + r;
        if (yy < 0 || yy >= surface->height) continue;
        for (int c_bit = 0; c_bit < 8; c_bit++) {
            int xx = x + c_bit;
            if (xx < 0 || xx >= surface->width) continue;
            uint32_t color = (bitmap[r] & (1 << c_bit)) ? 
                ((fg.r << 16) | (fg.g << 8) | fg.b) : 
                ((bg.r << 16) | (bg.g << 8) | bg.b);
            surface->buffer[yy * surface->width + xx] = color;
        }
    }
}

void surface_draw_text(gui_surface_t *surface, int x, int y, const char *text, gui_color_t fg, gui_color_t bg) {
    int cx = x;
    while (*text) {
        if (*text == '\n') {
            cx = x;
            y += GUI_FONT_HEIGHT;
        } else {
            surface_draw_char(surface, cx, y, *text, fg, bg);
            cx += GUI_FONT_WIDTH;
        }
        text++;
    }
}

void surface_blit_to_fb(gui_surface_t *surface) {
    fb_info_t *fb = fb_get_info();
    if (!fb || !surface || !surface->buffer) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)fb->virt_addr;
    
    for (int y = 0; y < surface->height; y++) {
        int fb_y = surface->y + y;
        if (fb_y < 0 || fb_y >= (int)fb->height) continue;
        if (surface->y < 0) continue;
        
        for (int x = 0; x < surface->width; x++) {
            int fb_x = surface->x + x;
            if (fb_x < 0 || fb_x >= (int)fb->width) continue;
            if (surface->x < 0) continue;
            
            target[fb_y * fb->width + fb_x] = surface->buffer[y * surface->width + x];
        }
    }
    
    // Mark surface area as damaged
    fb_add_damage(surface->x, surface->y, surface->width, surface->height);
}

void surface_draw_vline(gui_surface_t *surface, int x, int y, int h, gui_color_t color) {
    if (!surface || !surface->buffer || h <= 0) return;
    
    int y1 = y < 0 ? 0 : y;
    int y2 = (y + h > surface->height) ? surface->height : (y + h);
    if (x < 0 || x >= surface->width || y1 >= y2) return;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    for (int yy = y1; yy < y2; yy++) {
        surface->buffer[yy * surface->width + x] = color32;
    }
    
    // Damage will be marked when surface is blitted to framebuffer
}

void surface_draw_hline(gui_surface_t *surface, int x, int y, int w, gui_color_t color) {
    if (!surface || !surface->buffer || w <= 0) return;
    
    int x1 = x < 0 ? 0 : x;
    int x2 = (x + w > surface->width) ? surface->width : (x + w);
    if (y < 0 || y >= surface->height || x1 >= x2) return;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    uint32_t *row = surface->buffer + y * surface->width + x1;
    for (int xx = x1; xx < x2; xx++) {
        row[xx - x1] = color32;
    }
    
    // Damage will be marked when surface is blitted to framebuffer
}

// ============================================================================
// Modern UI Primitives for Surfaces
// ============================================================================

void surface_fill_rounded_rect(gui_surface_t *surface, int x, int y, int w, int h, int radius, gui_color_t color) {
    if (!surface || !surface->buffer || w <= 0 || h <= 0 || radius < 0) return;
    
    // Clamp radius
    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    if (radius == 0) {
        surface_fill_rect(surface, x, y, w, h, color);
        return;
    }
    
    // Fill main body
    for (int r = radius; r < h - radius; r++) {
        for (int c = 0; c < w; c++) {
            int sx = x + c;
            int sy = y + r;
            if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                surface->buffer[sy * surface->width + sx] = color32;
        }
    }
    
    // Fill top and bottom segments
    for (int r = 0; r < radius; r++) {
        for (int c = radius; c < w - radius; c++) {
            int sx = x + c;
            int sy = y + r;
            if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                surface->buffer[sy * surface->width + sx] = color32;
            sy = y + h - 1 - r;
            if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                surface->buffer[sy * surface->width + sx] = color32;
        }
    }
    
    // Fill rounded corners
    for (int r = 0; r < radius; r++) {
        for (int c = 0; c < radius; c++) {
            int dx = radius - c;
            int dy = radius - r;
            if (dx * dx + dy * dy <= radius * radius) {
                int sx = x + c;
                int sy = y + r;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                sx = x + w - 1 - c;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                sy = y + h - 1 - r;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                sx = x + c;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
            }
        }
    }
}

void surface_draw_rounded_rect(gui_surface_t *surface, int x, int y, int w, int h, int radius, gui_color_t color) {
    if (!surface || !surface->buffer || w <= 0 || h <= 0 || radius < 0) return;
    
    // Clamp radius
    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    
    uint32_t color32 = surface_color_to_uint32(color);
    
    if (radius == 0) {
        surface_draw_rect(surface, x, y, w, h, color);
        return;
    }
    
    // Draw rounded corners (simplified)
    for (int r = 0; r < radius; r++) {
        for (int c = 0; c < radius; c++) {
            int dx = radius - c;
            int dy = radius - r;
            if (dx * dx + dy * dy <= radius * radius) {
                // Top-left
                int sx = x + c;
                int sy = y + r;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                // Top-right
                sx = x + w - 1 - c;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                // Bottom-left
                sy = y + h - 1 - r;
                sx = x + c;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
                // Bottom-right
                sx = x + w - 1 - c;
                if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
                    surface->buffer[sy * surface->width + sx] = color32;
            }
        }
    }
    
    // Draw edges
    for (int c = radius; c < w - radius; c++) {
        int sx = x + c;
        int sy = y;
        if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
            surface->buffer[sy * surface->width + sx] = color32;
        sy = y + h - 1;
        if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
            surface->buffer[sy * surface->width + sx] = color32;
    }
    
    for (int r = radius; r < h - radius; r++) {
        int sx = x;
        int sy = y + r;
        if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
            surface->buffer[sy * surface->width + sx] = color32;
        sx = x + w - 1;
        if (sx >= 0 && sx < surface->width && sy >= 0 && sy < surface->height)
            surface->buffer[sy * surface->width + sx] = color32;
    }
}