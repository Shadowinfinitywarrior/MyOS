#ifndef GUI_LOGO_H
#define GUI_LOGO_H

#include "surface.h"
#include "../include/types.h"

/* MyOS Signature Planetary Orbital Ribbon 'M' Logo Engine
 * Renders the 3D gradient ribbon 'M' with celestial orbital ring and glowing satellite.
 * Designed to match the MyOS reference design specifications. */

void logo_draw_surface(surface_t *s, int cx, int cy, int size);
void logo_draw_buffer(uint32_t *bb, int stride, int scr_w, int scr_h, int cx, int cy, int size);

/* Render full brand lockup: Logo mark + "MyOS" typography + optional subtitle */
void logo_draw_badge(uint32_t *bb, int stride, int scr_w, int scr_h,
                     int x, int y, int size, bool with_text, bool with_subtitle);

#endif
