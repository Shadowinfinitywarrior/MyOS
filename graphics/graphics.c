#include "graphics.h"
#include "../lib/string.h"
#include "../drivers/font8x8_basic.h"
#include "../drivers/framebuffer.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static fb_info_t *g_fb = NULL;
static bool g_graphics_initialized = false;

void gui_graphics_init(void) {
    if (g_graphics_initialized) return;
    
    fb_init();
    g_fb = fb_get_info();
    g_graphics_initialized = true;
}

fb_info_t *gui_get_fb_info(void) {
    return g_fb;
}

void gui_clear(gui_color_t color) {
    if (!g_graphics_initialized) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    uint32_t fb_color = (color.r << 16) | (color.g << 8) | color.b;
    
    for (uint32_t i = 0; i < g_fb->width * g_fb->height; i++) {
        target[i] = fb_color;
    }
    
    // Mark full screen as damaged
    fb_add_damage(0, 0, g_fb->width, g_fb->height);
}

void gui_put_pixel(int x, int y, gui_color_t color) {
    if (!g_graphics_initialized || !g_fb) return;
    if (x < 0 || x >= (int)g_fb->width || y < 0 || y >= (int)g_fb->height) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    uint32_t fb_color = (color.r << 16) | (color.g << 8) | color.b;
    target[y * g_fb->width + x] = fb_color;
    
    // Mark single pixel as damaged
    fb_add_damage(x, y, 1, 1);
}

gui_color_t gui_get_pixel(int x, int y) {
    gui_color_t c = {0, 0, 0, 0};
    if (!g_graphics_initialized || !g_fb) return c;
    if (x < 0 || x >= (int)g_fb->width || y < 0 || y >= (int)g_fb->height) return c;
    uint32_t fb_color = fb_get_pixel(x, y);
    c.r = (fb_color >> 16) & 0xFF;
    c.g = (fb_color >> 8) & 0xFF;
    c.b = fb_color & 0xFF;
    c.a = 255;
    return c;
}

void gui_fill_rect(int x, int y, int w, int h, gui_color_t color) {
    if (!g_graphics_initialized || !g_fb) return;
    if (w <= 0 || h <= 0) return;
    
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w > (int)g_fb->width) ? (int)g_fb->width : (x + w);
    int y2 = (y + h > (int)g_fb->height) ? (int)g_fb->height : (y + h);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    uint32_t fb_color = (color.r << 16) | (color.g << 8) | color.b;
    
    for (int yy = y1; yy < y2; yy++) {
        uint32_t *row = target + yy * g_fb->width + x1;
        for (int xx = x1; xx < x2; xx++) {
            *row++ = fb_color;
        }
    }
    
    // Mark region as damaged
    fb_add_damage(x1, y1, x2 - x1, y2 - y1);
}

void gui_draw_rect(int x, int y, int w, int h, gui_color_t color) {
    if (w <= 0 || h <= 0) return;
    gui_draw_hline(x, y, w, color);
    gui_draw_hline(x, y + h - 1, w, color);
    gui_draw_vline(x, y, h, color);
    gui_draw_vline(x + w - 1, y, h, color);
}

void gui_draw_hline(int x, int y, int w, gui_color_t color) {
    if (w <= 0) return;
    int x1 = x < 0 ? 0 : x;
    int x2 = (x + w > (int)g_fb->width) ? (int)g_fb->width : (x + w);
    if (y < 0 || y >= (int)g_fb->height || x1 >= x2) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    uint32_t fb_color = (color.r << 16) | (color.g << 8) | color.b;
    uint32_t *row = target + y * g_fb->width + x1;
    for (int i = x1; i < x2; i++) {
        *row++ = fb_color;
    }
    
    // Mark region as damaged
    fb_add_damage(x1, y, x2 - x1, 1);
}

void gui_draw_vline(int x, int y, int h, gui_color_t color) {
    if (h <= 0) return;
    int y1 = y < 0 ? 0 : y;
    int y2 = (y + h > (int)g_fb->height) ? (int)g_fb->height : (y + h);
    if (x < 0 || x >= (int)g_fb->width || y1 >= y2) return;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    uint32_t fb_color = (color.r << 16) | (color.g << 8) | color.b;
    for (int yy = y1; yy < y2; yy++) {
        target[yy * g_fb->width + x] = fb_color;
    }
    
    // Mark region as damaged
    fb_add_damage(x, y1, 1, y2 - y1);
}

void gui_draw_line(int x1, int y1, int x2, int y2, gui_color_t color) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    
    // Calculate bounding box for damage
    int min_x = (x1 < x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_x = (x1 > x2) ? x1 : x2;
    int max_y = (y1 > y2) ? y1 : y2;
    
    while (true) {
        gui_put_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
    
    // Mark bounding box as damaged
    fb_add_damage(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
}

void gui_draw_char(int x, int y, char c, gui_color_t fg, gui_color_t bg) {
    if (!g_graphics_initialized) return;
    if ((unsigned char)c >= 128) c = '?';
    char *bitmap = font8x8_basic[(int)c];
    uint32_t fg_color = (fg.r << 16) | (fg.g << 8) | fg.b;
    uint32_t bg_color = (bg.r << 16) | (bg.g << 8) | bg.b;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    for (int r = 0; r < 8; r++) {
        for (int c_bit = 0; c_bit < 8; c_bit++) {
            uint32_t color = (bitmap[r] & (1 << c_bit)) ? fg_color : bg_color;
            int fx = x + c_bit;
            int fy = y + r;
            if (fx >= 0 && fx < (int)g_fb->width && fy >= 0 && fy < (int)g_fb->height) {
                target[fy * g_fb->width + fx] = color;
            }
        }
    }
    
    // Mark character area as damaged
    fb_add_damage(x, y, 8, 8);
}

void gui_draw_text(int x, int y, const char *text, gui_color_t fg, gui_color_t bg) {
    int cx = x;
    while (*text) {
        if (*text == '\n') {
            cx = x;
            y += GUI_FONT_HEIGHT;
        } else {
            gui_draw_char(cx, y, *text, fg, bg);
            cx += GUI_FONT_WIDTH;
        }
        text++;
    }
    
    // Mark text area as damaged (simplified)
    fb_add_damage(x, y, cx - x, GUI_FONT_HEIGHT);
}

int gui_text_width(const char *text) {
    int len = 0;
    while (*text && *text != '\n') { len++; text++; }
    return len * GUI_FONT_WIDTH;
}

int gui_text_height(void) {
    return GUI_FONT_HEIGHT;
}

void gui_blit(int dst_x, int dst_y, int src_x, int src_y, int w, int h, uint32_t *src_buffer) {
    if (!g_graphics_initialized || !g_fb || !src_buffer) return;
    if (w <= 0 || h <= 0) return;
    
    int x1 = dst_x < 0 ? 0 : dst_x;
    int y1 = dst_y < 0 ? 0 : dst_y;
    int x2 = (dst_x + w > (int)g_fb->width) ? (int)g_fb->width : (dst_x + w);
    int y2 = (dst_y + h > (int)g_fb->height) ? (int)g_fb->height : (dst_y + h);
    
    if (x1 >= x2 || y1 >= y2) return;
    
    int src_w = x2 - x1;
    int src_h = y2 - y1;
    int src_x_adj = src_x + (x1 - dst_x);
    int src_y_adj = src_y + (y1 - dst_y);
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    for (int yy = 0; yy < src_h; yy++) {
        uint32_t *dst_row = target + (y1 + yy) * g_fb->width + x1;
        uint32_t *src_row = src_buffer + (src_y_adj + yy) * w + src_x_adj;
        for (int xx = 0; xx < src_w; xx++) {
            *dst_row++ = src_row[xx];
        }
    }
    
    // Mark blitted region as damaged
    fb_add_damage(x1, y1, src_w, src_h);
}

// ============================================================================
// Modern UI Primitives
// ============================================================================

void gui_draw_rounded_rect(int x, int y, int w, int h, int radius, gui_color_t color) {
    if (w <= 0 || h <= 0 || radius < 0) return;
    
    // Clamp radius to half of smaller dimension
    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    
    uint32_t color32 = (color.r << 16) | (color.g << 8) | color.b;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    if (radius == 0) {
        // Draw regular rectangle
        gui_draw_rect(x, y, w, h, color);
        return;
    }
    
    // Draw rounded corners (simplified - using quarter circles)
    for (int r = 0; r < radius; r++) {
        for (int c = 0; c < radius; c++) {
            // Check if point is in quarter circle
            int dx = radius - c;
            int dy = radius - r;
            if (dx * dx + dy * dy <= radius * radius) {
                // Top-left corner
                if (x + c >= 0 && y + r >= 0)
                    target[(y + r) * g_fb->width + (x + c)] = color32;
                // Top-right corner
                if (x + w - 1 - c >= 0 && y + r >= 0)
                    target[(y + r) * g_fb->width + (x + w - 1 - c)] = color32;
                // Bottom-left corner
                if (x + c >= 0 && y + h - 1 - r >= 0)
                    target[(y + h - 1 - r) * g_fb->width + (x + c)] = color32;
                // Bottom-right corner
                if (x + w - 1 - c >= 0 && y + h - 1 - r >= 0)
                    target[(y + h - 1 - r) * g_fb->width + (x + w - 1 - c)] = color32;
            }
        }
    }
    
    // Draw edges
    for (int c = radius; c < w - radius; c++) {
        if (x + c >= 0 && y >= 0)
            target[y * g_fb->width + (x + c)] = color32;
        if (x + c >= 0 && y + h - 1 >= 0)
            target[(y + h - 1) * g_fb->width + (x + c)] = color32;
    }
    
    for (int r = radius; r < h - radius; r++) {
        if (x >= 0 && y + r >= 0)
            target[(y + r) * g_fb->width + x] = color32;
        if (x + w - 1 >= 0 && y + r >= 0)
            target[(y + r) * g_fb->width + (x + w - 1)] = color32;
    }
    
    fb_add_damage(x, y, w, h);
}

void gui_fill_rounded_rect(int x, int y, int w, int h, int radius, gui_color_t color) {
    if (w <= 0 || h <= 0 || radius < 0) return;
    
    // Clamp radius
    int max_radius = (w < h ? w : h) / 2;
    if (radius > max_radius) radius = max_radius;
    
    uint32_t color32 = (color.r << 16) | (color.g << 8) | color.b;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    if (radius == 0) {
        gui_fill_rect(x, y, w, h, color);
        return;
    }
    
    // Fill main body
    for (int r = radius; r < h - radius; r++) {
        for (int c = 0; c < w; c++) {
            if (x + c >= 0 && y + r >= 0 && x + c < (int)g_fb->width && y + r < (int)g_fb->height)
                target[(y + r) * g_fb->width + (x + c)] = color32;
        }
    }
    
    // Fill top and bottom segments
    for (int r = 0; r < radius; r++) {
        for (int c = radius; c < w - radius; c++) {
            if (x + c >= 0 && y + r >= 0 && x + c < (int)g_fb->width && y + r < (int)g_fb->height)
                target[(y + r) * g_fb->width + (x + c)] = color32;
            if (x + c >= 0 && y + h - 1 - r >= 0 && x + c < (int)g_fb->width && y + h - 1 - r < (int)g_fb->height)
                target[(y + h - 1 - r) * g_fb->width + (x + c)] = color32;
        }
    }
    
    // Fill rounded corners
    for (int r = 0; r < radius; r++) {
        for (int c = 0; c < radius; c++) {
            int dx = radius - c;
            int dy = radius - r;
            if (dx * dx + dy * dy <= radius * radius) {
                if (x + c >= 0 && y + r >= 0 && x + c < (int)g_fb->width && y + r < (int)g_fb->height)
                    target[(y + r) * g_fb->width + (x + c)] = color32;
                if (x + w - 1 - c >= 0 && y + r >= 0 && x + w - 1 - c < (int)g_fb->width && y + r < (int)g_fb->height)
                    target[(y + r) * g_fb->width + (x + w - 1 - c)] = color32;
                if (x + c >= 0 && y + h - 1 - r >= 0 && x + c < (int)g_fb->width && y + h - 1 - r < (int)g_fb->height)
                    target[(y + h - 1 - r) * g_fb->width + (x + c)] = color32;
                if (x + w - 1 - c >= 0 && y + h - 1 - r >= 0 && x + w - 1 - c < (int)g_fb->width && y + h - 1 - r < (int)g_fb->height)
                    target[(y + h - 1 - r) * g_fb->width + (x + w - 1 - c)] = color32;
            }
        }
    }
    
    fb_add_damage(x, y, w, h);
}

void gui_draw_shadow(int x, int y, int w, int h, int blur, gui_color_t color) {
    // Simplified shadow - draw offset dark rectangle
    (void)blur;  // TODO: Implement proper blur
    
    uint32_t shadow_color = (color.r << 16) | (color.g << 8) | color.b;
    int offset = 4;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    // Draw shadow offset
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int sx = x + dx + offset;
            int sy = y + dy + offset;
            if (sx >= 0 && sy >= 0 && sx < (int)g_fb->width && sy < (int)g_fb->height) {
                target[sy * g_fb->width + sx] = shadow_color;
            }
        }
    }
    
    fb_add_damage(x, y, w + offset, h + offset);
}

void gui_draw_glass_effect(int x, int y, int w, int h, gui_color_t bg_color, int alpha) {
    // Simplified glass effect - semi-transparent overlay
    (void)alpha;  // TODO: Implement proper alpha blending
    
    uint32_t bg32 = (bg_color.r << 16) | (bg_color.g << 8) | bg_color.b;
    
    // Use backbuffer if available
    uint32_t *target = (uint32_t *)fb_get_backbuffer();
    if (!target) target = (uint32_t *)g_fb->virt_addr;
    
    // Draw semi-transparent background
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int sx = x + dx;
            int sy = y + dy;
            if (sx >= 0 && sy >= 0 && sx < (int)g_fb->width && sy < (int)g_fb->height) {
                // Simple blend (50%)
                uint32_t existing = target[sy * g_fb->width + sx];
                uint32_t blended = ((existing & bg32) >> 1) + ((existing | bg32) >> 1);
                target[sy * g_fb->width + sx] = blended;
            }
        }
    }
    
    fb_add_damage(x, y, w, h);
}