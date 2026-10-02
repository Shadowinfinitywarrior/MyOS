/* mkbake - bake a TrueType font into kernel-side 1bpp glyph bitmaps.
 *
 * Host tool. Rasterises a fixed codepoint range at a fixed pixel size and
 * emits a C header of packed 1bpp glyphs plus per-glyph metrics, so the kernel
 * can draw proportional text with no TrueType code and no font parsing.
 *
 *   mkbake <font.ttf> <out.h> <name> <px> <first_cp> <last_cp>
 *
 * Output layout: glyphs are stored in codepoint order, each padded up to a
 * 4-byte row stride, concatenated into one byte array. Metrics are parallel
 * arrays (w, h, xoff, yoff, xadvance) indexed by (cp - first_cp).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

static unsigned char *bitmap;
static size_t bitmap_cap, bitmap_len;

static void emit(int byte) {
    if (bitmap_len == bitmap_cap) {
        bitmap_cap = bitmap_cap ? bitmap_cap * 2 : 65536;
        bitmap = realloc(bitmap, bitmap_cap);
        if (!bitmap) { fprintf(stderr, "oom\n"); exit(1); }
    }
    bitmap[bitmap_len++] = (unsigned char)byte;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: %s font.ttf out.h NAME px first_cp last_cp\n", argv[0]);
        return 1;
    }
    const char *ttf_path = argv[1];
    const char *out_path = argv[2];
    const char *name     = argv[3];
    int      px          = atoi(argv[4]);
    /* Base 0, not atoi: the block-element range we bake for the terminal banner
     * (U+2580..U+259F) is far past anything reachable by decimal. */
    int      first_cp    = (int)strtol(argv[5], NULL, 0);
    int      last_cp     = (int)strtol(argv[6], NULL, 0);

    FILE *f = fopen(ttf_path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", ttf_path); return 1; }
    fseek(f, 0, SEEK_END);
    long ttf_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ttf = malloc((size_t)ttf_size);
    if (!ttf || fread(ttf, 1, (size_t)ttf_size, f) != (size_t)ttf_size) {
        fprintf(stderr, "cannot read %s\n", ttf_path);
        return 1;
    }
    fclose(f);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        fprintf(stderr, "not a TTF/OTF we can parse: %s\n", ttf_path);
        return 1;
    }

    float scale = stbtt_ScaleForPixelHeight(&info, (float)px);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    int line_height = (int)((float)(ascent - descent + line_gap) * scale + 0.5f);
    if (line_height <= 0) line_height = px + 1;
    /* Distance from the baseline up to the top of the line box. stb reports
     * glyph box offsets relative to the baseline (negative = above it), but the
     * kernel draws each cell with y at the TOP of the line box, so baked yoff
     * values are rebased onto the line top. */
    int ascent_px = (int)((float)ascent * scale + 0.5f);

    int n = last_cp - first_cp + 1;
    int *w = calloc((size_t)n, sizeof(int)), *h = calloc((size_t)n, sizeof(int));
    int *xo = calloc((size_t)n, sizeof(int)), *yo = calloc((size_t)n, sizeof(int));
    int *xa = calloc((size_t)n, sizeof(int));
    if (!w || !h || !xo || !yo || !xa) { fprintf(stderr, "oom\n"); return 1; }

    /* The cell box that Unicode Block Elements have to fill, taken from the
     * space advance so it matches whatever the face uses for one column. */
    int adv_space = 0;
    stbtt_GetCodepointHMetrics(&info, ' ', &adv_space, NULL);
    int cell_w = (int)((float)adv_space * scale + 0.5f);
    if (cell_w <= 0) cell_w = px;

    for (int i = 0; i < n; i++) {
        int cp = first_cp + i;
        int gw = 0, gh = 0, gx0 = 0, gy0 = 0, adv = 0;
        stbtt_GetCodepointHMetrics(&info, cp, &adv, NULL);
        /* hmetrics are in font units; scale converts them to pixels. Passing the
         * raw font units to ScaleForMappingEmToPixels yields a scale *factor*,
         * not a distance, which truncates every advance to 0/1. */
        xa[i] = (int)((float)adv * scale + 0.5f);
        int is_block = (cp >= 0x2580 && cp <= 0x259F);
        if (cp == ' ') {
            /* space has no outline, but still needs its advance */
        } else if (is_block) {
            /* Block Elements are defined by covering the whole character cell,
             * so snap them to the cell box instead of using the tight box. The
             * TTF's own box does not: DejaVu's FULL BLOCK comes out 9x17 with a
             * -1 offset, which overflows the 8x15 cell and gets clipped into an
             * L-shape by the terminal's per-cell clipping. Snapping also makes
             * every block element tile seamlessly, which is the whole point of
             * using them to draw a banner. */
            gx0 = 0; gy0 = 0; gw = cell_w; gh = line_height;
        } else {
            /* This stb_truetype returns the box as (x0,y0,x1,y1) corners, with
             * y measured upwards from the baseline (so y0 is negative). */
            int x1 = 0, y1 = 0;
            stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &gx0, &gy0, &x1, &y1);
            gw = x1 - gx0;
            gh = y1 - gy0;
            if (gw < 0) gw = 0;
            if (gh < 0) gh = 0;
        }
        w[i] = gw; h[i] = gh; xo[i] = gx0;
        yo[i] = is_block ? 0 : (gy0 + ascent_px);
        if (xa[i] <= 0) xa[i] = gw > 0 ? gw + 1 : 4;

        int stride = (gw + 7) / 8;
        for (int row = 0; row < gh; row++)
            for (int s = 0; s < stride; s++) emit(0);
    }

    unsigned char *row8 = NULL;
    size_t row8_cap = 0;
    /* Supersampling factor for the downsample below. Very thin horizontal
     * strokes need it: '_' rasterises into a 2px-tall box, and at 1:1 the bar
     * lands between the sample rows, so every pixel comes back with zero
     * coverage and the glyph bakes as a blank hole in the ASCII art. */
    const int SS = 4;
    for (int i = 0; i < n; i++) {
        int cp = first_cp + i;
        if (cp == ' ' || w[i] == 0 || h[i] == 0) continue;
        size_t need = (size_t)w[i] * (size_t)h[i];
        if (need > row8_cap) { row8_cap = need; row8 = realloc(row8, row8_cap); }

        /* stbtt's rasteriser is 8-bits-per-pixel: its out_stride is a stride in
         * BYTES (one byte per pixel), not a 1bpp bit stride. Rasterising
         * straight into the packed array with a (w+7)/8 stride makes every row
         * overwrite the tail of the previous one and yields scrambled glyphs,
         * so rasterise wide first and pack the rows ourselves below. */
        int bw = w[i] * SS, bh = h[i] * SS;
        size_t need8 = (size_t)bw * (size_t)bh;
        if (need8 > row8_cap) { row8_cap = need8; row8 = realloc(row8, row8_cap); }
        stbtt_MakeCodepointBitmap(&info, row8, bw, bh, bw, scale * SS, scale * SS, cp);

        size_t off = 0;
        for (int k = 0; k < i; k++) off += (size_t)(((w[k] + 7) / 8) * h[k]);
        unsigned char *dst = bitmap + off;
        int stride = (w[i] + 7) / 8;
        for (int r = 0; r < h[i]; r++) {
            const unsigned char *src = row8 + (size_t)r * SS * bw;
            unsigned char *out = dst + (size_t)r * stride;
            for (int k = 0; k < stride; k++) {
                unsigned char byte = 0;
                for (int b = 0; b < 8; b++) {
                    int px = k * 8 + b;
                    if (px >= w[i]) continue;
                    /* Box-filter the SSxSS block down to one target pixel. */
                    unsigned sum = 0;
                    for (int sy = 0; sy < SS; sy++) {
                        const unsigned char *srow = src + (size_t)sy * bw;
                        for (int sx = 0; sx < SS; sx++) sum += srow[px * SS + sx];
                    }
                    if (sum >= (unsigned)(128 * SS * SS)) byte |= (unsigned char)(1 << b);
                }
                out[k] = byte;
            }
        }
    }
    free(row8);

    FILE *o = fopen(out_path, "w");
    if (!o) { fprintf(stderr, "cannot write %s\n", out_path); return 1; }

    fprintf(o, "/* Generated by tools/mkbake - do not edit.\n");
    fprintf(o, " * source: %s\n * size: %dpx  codepoints: %d..%d\n */\n",
            ttf_path, px, first_cp, last_cp);
    fprintf(o, "#ifndef FONT_%s_H\n#define FONT_%s_H\n\n", name, name);
    fprintf(o, "#include \"../include/types.h\"\n\n");
    fprintf(o, "#define %s_FIRST_CP   %d\n", name, first_cp);
    fprintf(o, "#define %s_LAST_CP    %d\n", name, last_cp);
    fprintf(o, "#define %s_GLYPH_COUNT %d\n", name, n);
    fprintf(o, "#define %s_PIXEL_SIZE %d\n", name, px);
    fprintf(o, "#define %s_LINE_HEIGHT %d\n\n", name, line_height);

    fprintf(o, "/* Parallel per-glyph metric arrays, indexed by (cp - %s_FIRST_CP). */\n", name);
    fprintf(o, "static const uint16_t %s_gw[] = {", name);
    for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", w[i]);
    fprintf(o, "};\n");
    fprintf(o, "static const uint16_t %s_gh[] = {", name);
    for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", h[i]);
    fprintf(o, "};\n");
    fprintf(o, "static const int8_t %s_xo[] = {", name);
    for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", xo[i]);
    fprintf(o, "};\n");
    fprintf(o, "static const int8_t %s_yo[] = {", name);
    for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", yo[i]);
    fprintf(o, "};\n");
    fprintf(o, "static const uint16_t %s_xa[] = {", name);
    for (int i = 0; i < n; i++) fprintf(o, "%s%d", i ? "," : "", xa[i]);
    fprintf(o, "};\n\n");

    /* Byte offset of each glyph within the concatenated bitmap. Baking this in
     * removes the prefix sum the kernel would otherwise recompute per draw. */
    fprintf(o, "static const uint32_t %s_bo[] = {", name);
    {
        uint32_t off = 0;
        for (int i = 0; i < n; i++) {
            fprintf(o, "%s%u", i ? "," : "", off);
            off += (uint32_t)(((w[i] + 7) / 8) * h[i]);
        }
    }
    fprintf(o, "};\n\n");

    fprintf(o, "/* 1bpp glyph bitmaps, rows padded to whole bytes, glyphs in codepoint order. */\n");
    fprintf(o, "static const uint8_t %s_bitmap[%zu] = {\n", name, bitmap_len);
    for (size_t i = 0; i < bitmap_len; i++) {
        if ((i % 16) == 0) fprintf(o, "\n   ");
        fprintf(o, " 0x%02x,", bitmap[i]);
    }
    fprintf(o, "\n};\n\n");
    fprintf(o, "#endif\n");
    fclose(o);

    fprintf(stderr, "mkbake: %s -> %s  %d glyphs, %zu bytes of bitmap, line_height=%d\n",
            ttf_path, out_path, n, bitmap_len, line_height);
    return 0;
}
