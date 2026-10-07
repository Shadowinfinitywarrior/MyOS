#ifndef GUI_SURFACE_H
#define GUI_SURFACE_H

#include "blit.h"
#include "rect.h"

/* An off-screen ARGB surface. Windows own one; the compositor copies it to the
 * screen. Damage is tracked so a frame only repaints what changed. */
typedef struct surface {
    uint32_t *pixels;
    int       w, h;
    int       pitch;          /* bytes per row */
    rect_t    damage[32];
    int       damage_count;
    bool      full_damage;
} surface_t;

surface_t *surface_create(int w, int h);
void       surface_destroy(surface_t *s);
void       surface_clear(surface_t *s, color_t c);

void surface_damage(surface_t *s, const rect_t *r);
void surface_damage_all(surface_t *s);
void surface_clear_damage(surface_t *s);
/* Union of everything damaged since the last clear. */
bool surface_damage_rect(surface_t *s, rect_t *out);

/* Drawing primitives. Coordinates are surface-local. */
void surface_fill_rect(surface_t *s, const rect_t *r, color_t c);
void surface_gradient_v(surface_t *s, const rect_t *r, color_t top, color_t bottom);
void surface_rect_outline(surface_t *s, const rect_t *r, color_t c, int thickness);
void surface_rounded_fill(surface_t *s, const rect_t *r, int radius, color_t c);
void surface_rounded_outline(surface_t *s, const rect_t *r, int radius, color_t c,
                             int thickness);
void surface_horizontal_gradient(surface_t *s, const rect_t *r, color_t left, color_t right);
void surface_blit_surface(surface_t *dst, const surface_t *src, int x, int y);
void surface_pixel(surface_t *s, int x, int y, color_t c);
void surface_pixel_blend(surface_t *s, int x, int y, color_t c, uint32_t alpha);

/* Present a screen-space rect of this surface at (dx,dy) on the back buffer. */
void surface_present(surface_t *s, int dx, int dy, const rect_t *src);

#endif
