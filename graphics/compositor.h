#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "graphics.h"
#include "surface.h"

#define COMPOSITOR_MAX_SURFACES 32

typedef struct compositor {
    gui_surface_t *surfaces;
    gui_surface_t *focused_surface;
    gui_surface_t *mouse_cursor_surface;
    int mouse_x;
    int mouse_y;
    bool mouse_visible;
    gui_color_t background_color;
    int fb_width;
    int fb_height;
} compositor_t;

compositor_t *compositor_create(int width, int height);
void compositor_destroy(compositor_t *comp);
void compositor_set_background(compositor_t *comp, gui_color_t color);
gui_surface_t *compositor_add_surface(compositor_t *comp, gui_surface_t *surface);
void compositor_remove_surface(compositor_t *comp, gui_surface_t *surface);
void compositor_set_focus(compositor_t *comp, gui_surface_t *surface);
void compositor_set_mouse(compositor_t *comp, int x, int y, bool visible);
void compositor_set_mouse_cursor(compositor_t *comp, gui_surface_t *cursor);
void compositor_render(compositor_t *comp);
void compositor_render_full(compositor_t *comp);

void compositor_redraw_region(compositor_t *comp, int x, int y, int w, int h);

#endif