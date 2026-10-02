#ifndef GUI_TEXT_H
#define GUI_TEXT_H

#include "blit.h"
#include "surface.h"

/* A baked 1bpp font: the arrays produced by tools/mkbake. All three faces share
 * this layout, so the renderer takes them uniformly. */
typedef struct baked_font {
    int           first_cp;
    int           glyph_count;
    int           line_height;
    const uint16_t *gw, *gh;     /* bitmap w/h per glyph            */
    const int8_t  *xo, *yo;      /* bitmap offset from pen position */
    const uint16_t *xa;          /* pen advance per glyph           */
    const uint32_t *bo;          /* byte offset into the bitmap    */
    const uint8_t *bitmap;       /* packed 1bpp, rows padded to bytes */
} baked_font_t;

/* ui  = proportional, regular weight (labels, menus, body text)
 * mono= proportional but fixed-advance (terminal content)
 * bold= proportional, bold weight (titles, active tab labels) */
const baked_font_t *font_ui(void);
const baked_font_t *font_mono(void);
const baked_font_t *font_bold(void);
const baked_font_t *font_blocks(void);

/* The face that contains `cp`, or NULL if none does. */
const baked_font_t *font_for_cp(uint32_t cp);

/* All text entry points return the pen x after the last glyph, so callers can
 * chain runs and measure without a separate pass. */
int text_draw(surface_t *s, const baked_font_t *f, int x, int y, const char *str,
              color_t color);

/* Draw one codepoint at (x,y), using whichever face contains it and `fallback`
 * when none does. For callers that drive their own character grid (the
 * terminal), where a string cannot be handed over because different cells may
 * need different faces. */
int text_draw_cp(surface_t *s, const baked_font_t *fallback, int x, int y,
                 uint32_t cp, color_t color);
int text_draw_n(surface_t *s, const baked_font_t *f, int x, int y, const char *str,
                int max_bytes, color_t color);
int text_width(const baked_font_t *f, const char *str);
int text_height(const baked_font_t *f);

/* Draw with a one-pixel drop shadow, used for text over wallpaper and video. */
int text_draw_shadow(surface_t *s, const baked_font_t *f, int x, int y,
                     const char *str, color_t color, color_t shadow);

/* UTF-8 decode; returns bytes consumed and stores the codepoint. Handles the
 * BMP directly. Invalid sequences decode as U+FFFD consuming one byte. */
int utf8_decode(const char *s, uint32_t *cp);

#endif
