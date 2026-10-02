#include "blit.h"
#include "../lib/string.h"
#include "../include/emmintrin.h"

static inline uint32_t lerp_channel(uint32_t a, uint32_t b, uint32_t t) {
    return a + (((b - a) * t) >> 8);
}

static inline uint32_t blend_pixel(uint32_t d, uint32_t s, uint32_t alpha) {
    uint32_t r = lerp_channel((d >> 16) & 0xFF, (s >> 16) & 0xFF, alpha);
    uint32_t g = lerp_channel((d >> 8) & 0xFF, (s >> 8) & 0xFF, alpha);
    uint32_t b = lerp_channel(d & 0xFF, s & 0xFF, alpha);
    return (r << 16) | (g << 8) | b;
}

void blit_fill(uint32_t *dst, int dst_pitch, const rect_t *r, color_t c) {
    if (rect_is_empty(r)) return;
    uint8_t *base = (uint8_t *)dst;

    /* 128-bit stores: the back buffer is RAM so this is the fast path that
     * makes full-window repaints affordable. */
    __m128i v = _mm_set1_epi32((int)c);
    for (int y = 0; y < r->h; y++) {
        uint32_t *row = (uint32_t *)(base + (size_t)(r->y + y) * dst_pitch + (size_t)r->x * 4);
        int x = 0;
        for (; x + 4 <= r->w; x += 4) {
            _mm_storeu_si128((__m128i *)(row + x), v);
        }
        for (; x < r->w; x++) row[x] = c;
    }
}

void blit_copy(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
               const rect_t *r) {
    if (rect_is_empty(r)) return;
    for (int y = 0; y < r->h; y++) {
        uint32_t *d = (uint32_t *)((uint8_t *)dst + (size_t)(r->y + y) * dst_pitch + (size_t)r->x * 4);
        const uint32_t *s = (const uint32_t *)((const uint8_t *)src +
                             (size_t)(r->y + y) * src_pitch + (size_t)r->x * 4);
        memcpy(d, s, (size_t)r->w * 4);
    }
}

void blit_copy_offset(uint32_t *dst, int dst_pitch, int dx, int dy,
                      const uint32_t *src, int src_pitch, int sx, int sy,
                      int w, int h) {
    if (w <= 0 || h <= 0) return;
    for (int y = 0; y < h; y++) {
        uint32_t *d = (uint32_t *)((uint8_t *)dst + (size_t)(dy + y) * dst_pitch + (size_t)dx * 4);
        const uint32_t *s = (const uint32_t *)((const uint8_t *)src +
                             (size_t)(sy + y) * src_pitch + (size_t)sx * 4);
        memcpy(d, s, (size_t)w * 4);
    }
}

void blit_copy_alpha(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
                     const rect_t *r) {
    if (rect_is_empty(r)) return;
    for (int y = 0; y < r->h; y++) {
        uint32_t *d = (uint32_t *)((uint8_t *)dst + (size_t)(r->y + y) * dst_pitch + (size_t)r->x * 4);
        const uint32_t *s = (const uint32_t *)((const uint8_t *)src +
                             (size_t)(r->y + y) * src_pitch + (size_t)r->x * 4);
        for (int x = 0; x < r->w; x++) {
            /* A zero word is the surface's "transparent" sentinel. */
            if (s[x]) d[x] = s[x];
        }
    }
}

void blit_blend(uint32_t *dst, int dst_pitch, const uint32_t *src, int src_pitch,
                const rect_t *r, uint32_t alpha) {
    if (rect_is_empty(r) || alpha == 0) return;
    if (alpha > 255) alpha = 255;
    for (int y = 0; y < r->h; y++) {
        uint32_t *d = (uint32_t *)((uint8_t *)dst + (size_t)(r->y + y) * dst_pitch + (size_t)r->x * 4);
        const uint32_t *s = (const uint32_t *)((const uint8_t *)src +
                             (size_t)(r->y + y) * src_pitch + (size_t)r->x * 4);
        for (int x = 0; x < r->w; x++) {
            d[x] = blend_pixel(d[x], s[x], alpha);
        }
    }
}

void blit_vertical_gradient(uint32_t *dst, int dst_pitch, const rect_t *r,
                            color_t top, color_t bottom) {
    if (rect_is_empty(r)) return;
    for (int y = 0; y < r->h; y++) {
        uint32_t t = r->h > 1 ? (uint32_t)((uint64_t)y * 255 / (uint64_t)(r->h - 1)) : 0;
        uint32_t c = blend_pixel(top, bottom, t);
        uint32_t *row = (uint32_t *)((uint8_t *)dst + (size_t)(r->y + y) * dst_pitch + (size_t)r->x * 4);
        for (int x = 0; x < r->w; x++) row[x] = c;
    }
}

void blit_glyph_1bpp(uint32_t *dst, int dst_pitch, const uint8_t *bits,
                     int bw, int bh, color_t color) {
    int stride = (bw + 7) / 8;
    for (int row = 0; row < bh; row++) {
        const uint8_t *brow = bits + row * stride;
        uint32_t *d = (uint32_t *)((uint8_t *)dst + (size_t)row * dst_pitch);
        for (int col = 0; col < bw; col++) {
            if (brow[col >> 3] & (1 << (col & 7))) d[col] = color;
        }
    }
}
