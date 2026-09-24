#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    uint32_t codepoint;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t advance;
    int32_t bearing_x;
    int32_t bearing_y;
} glyph_meta_t;

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.ttf> <output_prefix>\n", argv[0]);
        return 1;
    }
    const char *ttf_path = argv[1];
    const char *prefix = argv[2];

    FILE *f = fopen(ttf_path, "rb");
    if (!f) { perror("fopen ttf"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ttf_buf = (unsigned char*)malloc(size);
    if (!ttf_buf) { fclose(f); return 1; }
    size_t __read = fread(ttf_buf, 1, size, f);
    (void)__read;
    fclose(f);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf_buf, stbtt_GetFontOffsetForIndex(ttf_buf,0))) {
        fprintf(stderr, "Failed to init font\n");
        return 1;
    }

    float scale = stbtt_ScaleForPixelHeight(&font, 64.0f);
    const int first = 32, last = 126;
    const int second_start = 0xA0, second_end = 0xFF;
    int codes[256];
    int code_count = 0;
    for (int i = first; i <= last; ++i) codes[code_count++] = i;
    for (int i = second_start; i <= second_end; ++i) codes[code_count++] = i;

    const int atlas_w = 2048, atlas_h = 2048;
    unsigned char *atlas = (unsigned char*)calloc(atlas_w * atlas_h, 1);
    if (!atlas) return 1;

    glyph_meta_t *metas = (glyph_meta_t*)malloc(sizeof(glyph_meta_t)*code_count);
    int pack_x = 0, pack_y = 0, row_h = 0;
    int meta_idx = 0;

    for (int i = 0; i < code_count; ++i) {
        int cp = codes[i];
        int w=0,h=0,xoff=0,yoff=0;
        unsigned char *sdf = stbtt_GetCodepointSDF(&font, scale, cp, 2, 128, 4.0f, &w, &h, &xoff, &yoff);
        if (!sdf) continue;
        if (pack_x + w > atlas_w) { pack_x = 0; pack_y += row_h + 2; row_h = 0; }
        if (pack_y + h > atlas_h) { fprintf(stderr, "Atlas overflow\n"); return 1; }
        for (int yy = 0; yy < h; ++yy) {
            memcpy(atlas + (pack_y + yy)*atlas_w + pack_x, sdf + yy*w, w);
        }
        int advanceW=0, lsb=0;
        int gidx = stbtt_FindGlyphIndex(&font, cp);
        stbtt_GetGlyphHMetrics(&font, gidx, &advanceW, &lsb);
        int advance = (int)(advanceW*scale + 0.5f);
        metas[meta_idx].codepoint = (uint32_t)cp;
        metas[meta_idx].x = pack_x;
        metas[meta_idx].y = pack_y;
        metas[meta_idx].w = w;
        metas[meta_idx].h = h;
        metas[meta_idx].advance = advance;
        metas[meta_idx].bearing_x = xoff;
        metas[meta_idx].bearing_y = yoff;
        meta_idx++;
        pack_x += w + 2;
        if (h > row_h) row_h = h;
        stbtt_FreeSDF(sdf, NULL);
    }

    char path[512];
    snprintf(path, sizeof(path), "%s.atlas", prefix);
    FILE *out = fopen(path, "wb");
    fwrite(atlas, 1, atlas_w*atlas_h, out);
    fclose(out);

    snprintf(path, sizeof(path), "%s.meta", prefix);
    out = fopen(path, "wb");
    uint32_t count = meta_idx;
    fwrite(&count, 4, 1, out);
    fwrite(metas, sizeof(glyph_meta_t), meta_idx, out);
    fclose(out);

    snprintf(path, sizeof(path), "%s_preview.ppm", prefix);
    out = fopen(path, "wb");
    fprintf(out, "P5\n%d %d\n255\n", atlas_w, atlas_h);
    fwrite(atlas, 1, atlas_w*atlas_h, out);
    fclose(out);

    printf("fontbake: %d glyphs -> %s.atlas %s.meta\n", meta_idx, prefix, prefix);
    free(atlas); free(metas); free(ttf_buf);
    return 0;
}
