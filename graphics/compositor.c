#include "compositor.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "graphics.h"
#include "surface.h"
#include "../drivers/framebuffer.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

compositor_t *compositor_create(int width, int height) {
    (void)width; (void)height;
    compositor_t *comp = (compositor_t *)kmalloc(sizeof(compositor_t));
    if (!comp) return NULL;

    comp->surfaces = NULL;
    comp->focused_surface = NULL;
    comp->mouse_cursor_surface = NULL;
    comp->mouse_x = 512;  // Start cursor in center of screen
    comp->mouse_y = 384;
    comp->mouse_visible = true;
    comp->background_color = COLOR_MYOS_DARK;
    comp->fb_width = 0;
    comp->fb_height = 0;

    return comp;
}

void compositor_destroy(compositor_t *comp) {
    if (!comp) return;
    gui_surface_t *s = comp->surfaces;
    while (s) {
        gui_surface_t *next = s->next;
        surface_destroy(s);
        s = next;
    }
    if (comp->mouse_cursor_surface) surface_destroy(comp->mouse_cursor_surface);
    kfree(comp);
}

void compositor_set_background(compositor_t *comp, gui_color_t color) {
    if (comp) comp->background_color = color;
}

gui_surface_t *compositor_add_surface(compositor_t *comp, gui_surface_t *surface) {
    if (!comp || !surface) return NULL;
    
    // Prevent duplicate entries from growing the list each frame
    gui_surface_t *s = comp->surfaces;
    while (s) {
        if (s == surface) {
            // Already in list; move to front to keep z-order fresh
            if (s != comp->surfaces) {
                if (s->prev) s->prev->next = s->next;
                if (s->next) s->next->prev = s->prev;
                s->next = comp->surfaces;
                if (comp->surfaces) comp->surfaces->prev = s;
                s->prev = NULL;
                comp->surfaces = s;
            }
            return surface;
        }
        s = s->next;
    }
    
    surface->next = comp->surfaces;
    if (comp->surfaces) comp->surfaces->prev = surface;
    comp->surfaces = surface;
    
    return surface;
}

void compositor_remove_surface(compositor_t *comp, gui_surface_t *surface) {
    if (!comp || !surface) return;
    
    if (surface->prev) surface->prev->next = surface->next;
    if (surface->next) surface->next->prev = surface->prev;
    if (comp->surfaces == surface) comp->surfaces = surface->next;
    if (comp->focused_surface == surface) comp->focused_surface = comp->surfaces;
    
    surface->next = NULL;
    surface->prev = NULL;
}

void compositor_set_focus(compositor_t *comp, gui_surface_t *surface) {
    if (!comp) return;
    
    if (comp->focused_surface) comp->focused_surface->focused = false;
    comp->focused_surface = surface;
    if (surface) surface->focused = true;
    
    // Move to front (top of z-order)
    if (surface && surface != comp->surfaces) {
        if (surface->prev) surface->prev->next = surface->next;
        if (surface->next) surface->next->prev = surface->prev;
        
        surface->next = comp->surfaces;
        surface->prev = NULL;
        if (comp->surfaces) comp->surfaces->prev = surface;
        comp->surfaces = surface;
    }
}

void compositor_set_mouse(compositor_t *comp, int x, int y, bool visible) {
    if (!comp) return;
    comp->mouse_x = x;
    comp->mouse_y = y;
    comp->mouse_visible = visible;
}

void compositor_set_mouse_cursor(compositor_t *comp, gui_surface_t *cursor) {
    if (comp) {
        if (comp->mouse_cursor_surface) surface_destroy(comp->mouse_cursor_surface);
        comp->mouse_cursor_surface = cursor;
    }
}

void compositor_render(compositor_t *comp) {
    if (!comp) return;

    fb_info_t *fb = fb_get_info();
    if (!fb) return;

    comp->fb_width = fb->width;
    comp->fb_height = fb->height;

    // Use backbuffer for rendering
    uint32_t *target = fb_get_backbuffer();
    if (!target) target = (uint32_t *)fb->virt_addr;

    // Clear with background color
    uint32_t bg_color = (comp->background_color.r << 16) | (comp->background_color.g << 8) | comp->background_color.b;
    for (uint32_t i = 0; i < fb->width * fb->height; i++) {
        target[i] = bg_color;
    }

    // Collect all visible surfaces in z-order (back to front)
    gui_surface_t *surfaces[COMPOSITOR_MAX_SURFACES];
    int count = 0;
    gui_surface_t *s = comp->surfaces;
    while (s && count < COMPOSITOR_MAX_SURFACES) {
        if (s->visible) surfaces[count++] = s;
        s = s->next;
    }

    // Render back to front
    for (int i = count - 1; i >= 0; i--) {
        if (surfaces[i] && surfaces[i]->visible) {
            // blit this surface to backbuffer
            for (int y = 0; y < surfaces[i]->height; y++) {
                int fb_y = surfaces[i]->y + y;
                if (fb_y < 0 || fb_y >= (int)fb->height) continue;
                if (surfaces[i]->y < 0) continue;

                for (int x = 0; x < surfaces[i]->width; x++) {
                    int fb_x = surfaces[i]->x + x;
                    if (fb_x < 0 || fb_x >= (int)fb->width) continue;
                    if (surfaces[i]->x < 0) continue;

                    uint32_t pixel = surfaces[i]->buffer[y * surfaces[i]->width + x];
                    // Simple alpha blending: skip fully transparent pixels
                    if (pixel != 0x00000000) {
                        target[fb_y * fb->width + fb_x] = pixel;
                    }
                }
            }
        }
    }

    // Draw mouse cursor last (on top of everything)
    if (comp->mouse_visible && comp->mouse_cursor_surface) {
        gui_surface_t *cursor = comp->mouse_cursor_surface;
        int cy = comp->mouse_y;

        for (int y = 0; y < cursor->height; y++) {
            int fy = cy + y;
            if (fy < 0 || fy >= (int)fb->height) continue;

            for (int x = 0; x < cursor->width; x++) {
                int fx = comp->mouse_x + x;
                if (fx < 0 || fx >= (int)fb->width) continue;

                uint32_t pixel = cursor->buffer[y * cursor->width + x];
                // Only draw non-transparent pixels (0x00000000 is transparent)
                if (pixel != 0x00000000) {
                    target[fy * fb->width + fx] = pixel;
                }
            }
        }
    }

    // Mark full screen as damaged for this frame
    fb_add_damage(0, 0, fb->width, fb->height);
}

void compositor_render_full(compositor_t *comp) {
    compositor_render(comp);
}

void compositor_redraw_region(compositor_t *comp, int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    // Full redraw for now
    compositor_render(comp);
}