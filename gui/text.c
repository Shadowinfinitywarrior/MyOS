#include "text.h"
#include "fonts/ui.h"
#include "fonts/mono.h"
#include "fonts/ubold.h"
#include "fonts/blocks.h"

static const baked_font_t ui_font = {
    UI_FIRST_CP, UI_GLYPH_COUNT, UI_LINE_HEIGHT,
    UI_gw, UI_gh, UI_xo, UI_yo, UI_xa, UI_bo, UI_bitmap
};

static const baked_font_t mono_font = {
    MONO_FIRST_CP, MONO_GLYPH_COUNT, MONO_LINE_HEIGHT,
    MONO_gw, MONO_gh, MONO_xo, MONO_yo, MONO_xa, MONO_bo, MONO_bitmap
};

static const baked_font_t bold_font = {
    UBOLD_FIRST_CP, UBOLD_GLYPH_COUNT, UBOLD_LINE_HEIGHT,
    UBOLD_gw, UBOLD_gh, UBOLD_xo, UBOLD_yo, UBOLD_xa, UBOLD_bo, UBOLD_bitmap
};

/* Block Elements live outside the 32..126 range the three text faces cover, so
 * they get their own face. It is baked cell-filling: every glyph is exactly one
 * character cell, which is what lets them tile into a banner. */
static const baked_font_t blocks_font = {
    BLOCKS_FIRST_CP, BLOCKS_GLYPH_COUNT, BLOCKS_LINE_HEIGHT,
    BLOCKS_gw, BLOCKS_gh, BLOCKS_xo, BLOCKS_yo, BLOCKS_xa, BLOCKS_bo, BLOCKS_bitmap
};

const baked_font_t *font_ui(void)    { return &ui_font; }
const baked_font_t *font_mono(void)  { return &mono_font; }
const baked_font_t *font_bold(void)  { return &bold_font; }
const baked_font_t *font_blocks(void) { return &blocks_font; }

/* The face that actually contains a codepoint, or NULL if none does. Callers
 * that draw a whole string stay on a single face; the terminal, which picks a
 * face per cell, needs this to fall back for block characters. */
const baked_font_t *font_for_cp(uint32_t cp) {
    if (cp >= (uint32_t)mono_font.first_cp &&
        cp < (uint32_t)(mono_font.first_cp + mono_font.glyph_count))
        return &mono_font;
    if (cp >= (uint32_t)blocks_font.first_cp &&
        cp < (uint32_t)(blocks_font.first_cp + blocks_font.glyph_count))
        return &blocks_font;
    return NULL;
}

int utf8_decode(const char *s, uint32_t *cp) {
    const uint8_t *u = (const uint8_t *)s;
    uint8_t c = u[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = (uint32_t)((c & 0x1F) << 6) | (uint32_t)(u[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = (uint32_t)((c & 0x0F) << 12) | (uint32_t)((u[1] & 0x3F) << 6) |
              (uint32_t)(u[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 &&
        (u[3] & 0xC0) == 0x80) {
        *cp = (uint32_t)((c & 0x07) << 18) | (uint32_t)((u[1] & 0x3F) << 12) |
              (uint32_t)((u[2] & 0x3F) << 6) | (uint32_t)(u[3] & 0x3F);
        return 4;
    }
    *cp = 0xFFFD;
    return 1;
}

/* Glyph bitmaps are laid out in codepoint order with rows padded to whole
 * bytes; the byte offset of each glyph is baked into the font header. */
static void draw_glyph(surface_t *s, const baked_font_t *f, int index, int x, int y,
                       color_t color) {
    if (index < 0 || index >= f->glyph_count) return;
    int gw = f->gw[index], gh = f->gh[index];
    if (gw <= 0 || gh <= 0) return;
    int gx = x + f->xo[index];
    int gy = y + f->yo[index];
    /* Clip to the surface before touching rows. */
    int sx = 0, sy = 0;
    if (gx < 0) { sx = -gx; gw += gx; gx = 0; }
    if (gy < 0) { sy = -gy; gh += gy; gy = 0; }
    if (gx + gw > s->w) gw = s->w - gx;
    if (gy + gh > s->h) gh = s->h - gy;
    if (gw <= 0 || gh <= 0) return;

    int stride = (f->gw[index] + 7) / 8;
    const uint8_t *bits = f->bitmap + f->bo[index] + (size_t)sy * stride;

    for (int row = 0; row < gh; row++) {
        const uint8_t *brow = bits + (size_t)row * stride;
        uint32_t *d = (uint32_t *)((uint8_t *)s->pixels +
                                   (size_t)(gy + row) * s->pitch + (size_t)gx * 4);
        for (int col = 0; col < gw; col++) {
            if (brow[(sx + col) >> 3] & (1 << ((sx + col) & 7))) d[col] = color;
        }
    }
}

int text_draw_n(surface_t *s, const baked_font_t *f, int x, int y, const char *str,
                int max_bytes, color_t color) {
    if (!s || !f || !str) return x;
    int pen = x;
    int i = 0;
    /* Derive the top of the face's range instead of hardcoding 126, so faces
     * that cover more (the block-element face) are not clipped by it. */
    uint32_t end = (uint32_t)(f->first_cp + f->glyph_count);
    while (str[i] && (max_bytes < 0 || i < max_bytes)) {
        uint32_t cp;
        i += utf8_decode(str + i, &cp);
        if (cp == '\n') { y += f->line_height; continue; }
        if (cp < (uint32_t)f->first_cp || cp >= end) continue;
        int index = (int)cp - f->first_cp;
        draw_glyph(s, f, index, pen, y, color);
        pen += f->xa[index];
    }
    return pen;
}

int text_draw(surface_t *s, const baked_font_t *f, int x, int y, const char *str,
              color_t color) {
    return text_draw_n(s, f, x, y, str, -1, color);
}

/* Draw exactly one codepoint, choosing whichever face contains it.
 *
 * The terminal stores a codepoint per cell and lays cells out on its own fixed
 * grid, so it cannot hand text_draw a string: it needs one glyph at one cell
 * origin, and it needs block characters to come from a different face than the
 * surrounding text. Returns the pen x after the glyph (or unchanged if no face
 * covers the codepoint). */
int text_draw_cp(surface_t *s, const baked_font_t *fallback, int x, int y,
                 uint32_t cp, color_t color) {
    if (!s) return x;
    const baked_font_t *f = font_for_cp(cp);
    if (!f) f = fallback;
    if (!f) return x;
    uint32_t end = (uint32_t)(f->first_cp + f->glyph_count);
    if (cp < (uint32_t)f->first_cp || cp >= end) return x;
    draw_glyph(s, f, (int)cp - f->first_cp, x, y, color);
    return x + f->xa[(int)cp - f->first_cp];
}

int text_draw_shadow(surface_t *s, const baked_font_t *f, int x, int y,
                     const char *str, color_t color, color_t shadow) {
    text_draw(s, f, x + 1, y + 1, str, shadow);
    return text_draw(s, f, x, y, str, color);
}

int text_width(const baked_font_t *f, const char *str) {
    if (!f || !str) return 0;
    int w = 0;
    for (int i = 0; str[i]; ) {
        uint32_t cp;
        i += utf8_decode(str + i, &cp);
        if (cp == '\n') continue;
        if (cp < (uint32_t)f->first_cp || cp > 126) continue;
        w += f->xa[cp - f->first_cp];
    }
    return w;
}

int text_height(const baked_font_t *f) {
    return f ? f->line_height : 0;
}
