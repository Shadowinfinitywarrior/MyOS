#ifndef GUI_BLIT_H
#define GUI_BLIT_H

#include "rect.h"

/* Colours are 0x00RRGGBB, matching the BGA LFB's little-endian BGRX layout. */
typedef uint32_t color_t;

#define RGB(r, g, b) ((color_t)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define RGBA(r, g, b, a) RGB(r, g, b)

/* All blits take a source and destination pixel pitch in bytes so they work on
 * the framebuffer back buffer, on surfaces, and on the LFB alike. */

void blit_fill(uint32_t *dst, int dst_pitch, const rect_t *r, color_t c);
void blit_copy(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
               const rect_t *r);
/* Copy a w*h block from (src,sx,sy) to (dst,dx,dy). Unlike blit_copy the
 * source and destination origins are independent, which is what a compositor
 * needs when a window is clipped at the screen edges. */
void blit_copy_offset(uint32_t *dst, int dst_pitch, int dx, int dy,
                      const uint32_t *src, int src_pitch, int sx, int sy,
                      int w, int h);
void blit_copy_alpha(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
                     const rect_t *r);
/* Alpha-blend src over dst, treating the top bit of each colour channel as a
 * coverage mask (used for 1bpp text antialiasing and window shadows). */
void blit_blend(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
                const rect_t *r, uint32_t alpha /* 0..255 */);
void blit_vertical_gradient(uint32_t *dst, int dst_pitch, const rect_t *r,
                            color_t top, color_t bottom);

/* 1bpp glyph blit: expands a packed monochrome bitmap at (bw,bh) into
 * (dst,color), advancing by the font's per-glyph advance. */
void blit_glyph_1bpp(uint32_t *dst, int dst_pitch, const uint8_t *bits,
                     int bw, int bh, color_t color);

#endif
