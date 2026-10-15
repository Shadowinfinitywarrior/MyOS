#include "glass.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../lib/string.h"

/* =========================================================================
 * MyOS Centralized Glassmorphism Design System Theme Instance
 * ========================================================================= */

glass_theme_t MyOSTheme = {
    .bg_deep                 = RGB(0x0C, 0x10, 0x1D),  /* #0C101D */
    .bg_mid                  = RGB(0x15, 0x1A, 0x30),  /* #151A30 */
    .glass_surface_primary   = RGB(0x17, 0x1D, 0x2B),  /* #171D2B */
    .glass_surface_sec       = RGB(0x22, 0x2A, 0x3B),  /* #222A3B */
    .accent_primary          = RGB(0x8B, 0x9D, 0xFF),  /* #8B9DFF */
    .accent_secondary        = RGB(0x65, 0xD9, 0xFF),  /* #65D9FF */
    .text_primary            = RGB(0xF4, 0xF6, 0xFF),  /* #F4F6FF */
    .text_secondary          = RGB(0xA8, 0xB0, 0xC5),  /* #A8B0C5 */
    .text_dim                = RGB(0x6E, 0x7C, 0x98),  /* #6E7C98 */
    .border_glass            = RGB(0xFF, 0xFF, 0xFF),  /* White 10-15% */
    .highlight_shine         = RGB(0xFF, 0xFF, 0xFF),  /* Specular white */
    .glow_ambient            = RGB(0x55, 0x88, 0xE5),

    .radius_window           = 16,
    .radius_panel            = 18,
    .radius_button           = 9,
    .radius_card             = 12,
    .radius_input            = 8,
    .radius_dock             = 24,
    .radius_pill             = 14,

    .alpha_window            = 225,
    .alpha_panel             = 180,
    .alpha_dock              = 165,
    .alpha_menu              = 200,
    .alpha_notification      = 195,
    .alpha_card              = 185,
    .alpha_button_hover      = 195,
    .alpha_border            = 36,   /* ~14% opacity */
    .alpha_shine             = 105,  /* ~41% opacity */

    .blur_enabled            = false,
    .blur_radius             = 0,
    .reduced_motion          = false,
    .reduced_effects         = false
};

void glass_init(void) {
    /* Baseline theme initialized */
}

void glass_set_blur_enabled(bool enabled) {
    MyOSTheme.blur_enabled = enabled;
}

bool glass_get_blur_enabled(void) {
    return MyOSTheme.blur_enabled;
}

void glass_set_reduced_motion(bool enabled) {
    MyOSTheme.reduced_motion = enabled;
}

bool glass_get_reduced_motion(void) {
    return MyOSTheme.reduced_motion;
}

void glass_set_reduced_effects(bool enabled) {
    MyOSTheme.reduced_effects = enabled;
    if (enabled) {
        MyOSTheme.blur_enabled = false;
    }
}

bool glass_get_reduced_effects(void) {
    return MyOSTheme.reduced_effects;
}

/* =========================================================================
 * Fast Hardware-Independent Alpha & Shape Helpers
 * ========================================================================= */

static inline void glass_blend(uint32_t *bb, int stride, int x, int y,
                               color_t c, uint32_t alpha, int scr_w, int scr_h) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h || alpha == 0) return;
    size_t idx = (size_t)y * (size_t)stride + (size_t)x;
    if (alpha >= 255) {
        bb[idx] = c;
        return;
    }
    uint32_t d = bb[idx];
    uint32_t inv = 256 - alpha;
    uint32_t r = (((d >> 16) & 0xFF) * inv + (((c >> 16) & 0xFF) * alpha)) >> 8;
    uint32_t g = (((d >> 8) & 0xFF) * inv + (((c >> 8) & 0xFF) * alpha)) >> 8;
    uint32_t b = ((d & 0xFF) * inv + ((c & 0xFF) * alpha)) >> 8;
    bb[idx] = (r << 16) | (g << 8) | b;
}

static void glass_fill_rect_aa(uint32_t *bb, int stride, const rect_t *r,
                               color_t c, int alpha, int scr_w, int scr_h) {
    if (r->w <= 0 || r->h <= 0 || alpha <= 0) return;
    int x0 = r->x < 0 ? 0 : r->x;
    int y0 = r->y < 0 ? 0 : r->y;
    int x1 = r->x + r->w > scr_w ? scr_w : r->x + r->w;
    int y1 = r->y + r->h > scr_h ? scr_h : r->y + r->h;
    if (x0 >= x1 || y0 >= y1) return;

    if (alpha >= 255) {
        for (int y = y0; y < y1; y++) {
            uint32_t *row = bb + (size_t)y * (size_t)stride;
            for (int x = x0; x < x1; x++) row[x] = c;
        }
        return;
    }

    uint32_t cr = (c >> 16) & 0xFF;
    uint32_t cg = (c >> 8) & 0xFF;
    uint32_t cb = c & 0xFF;
    uint32_t inv = 256 - alpha;

    for (int y = y0; y < y1; y++) {
        uint32_t *row = bb + (size_t)y * (size_t)stride;
        for (int x = x0; x < x1; x++) {
            uint32_t d = row[x];
            uint32_t r0 = (((d >> 16) & 0xFF) * inv + cr * alpha) >> 8;
            uint32_t g0 = (((d >> 8) & 0xFF) * inv + cg * alpha) >> 8;
            uint32_t b0 = ((d & 0xFF) * inv + cb * alpha) >> 8;
            row[x] = (r0 << 16) | (g0 << 8) | b0;
        }
    }
}

static void glass_round_rect_aa(uint32_t *bb, int stride, const rect_t *r,
                                int radius, color_t c, int alpha,
                                int scr_w, int scr_h) {
    if (r->w <= 0 || r->h <= 0 || alpha <= 0) return;
    if (radius <= 0) {
        glass_fill_rect_aa(bb, stride, r, c, alpha, scr_w, scr_h);
        return;
    }
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;

    /* Central cross bodies */
    rect_t body = { r->x, r->y + radius, r->w, r->h - radius * 2 };
    glass_fill_rect_aa(bb, stride, &body, c, alpha, scr_w, scr_h);
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, radius };
    glass_fill_rect_aa(bb, stride, &top, c, alpha, scr_w, scr_h);
    rect_t bot = { r->x + radius, r->y + r->h - radius, r->w - radius * 2, radius };
    glass_fill_rect_aa(bb, stride, &bot, c, alpha, scr_w, scr_h);

    /* 4x Subpixel Corner antialiasing */
    int r4 = radius * 4;
    int r4_sq = r4 * r4;
    int corners[4][2] = {
        { r->x + radius, r->y + radius },
        { r->x + r->w - radius - 1, r->y + radius },
        { r->x + radius, r->y + r->h - radius - 1 },
        { r->x + r->w - radius - 1, r->y + r->h - radius - 1 },
    };

    for (int k = 0; k < 4; k++) {
        int cx = corners[k][0], cy = corners[k][1];
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                int px = (k & 1) ? (cx + dx) : (cx - radius + 1 + dx);
                int py = (k & 2) ? (cy + dy) : (cy - radius + 1 + dy);
                if (px < 0 || py < 0 || px >= scr_w || py >= scr_h) continue;

                int sx = (px - cx) * 4;
                int sy = (py - cy) * 4;
                int cov = 0;
                if ((sx + 1) * (sx + 1) + (sy + 1) * (sy + 1) <= r4_sq) cov++;
                if ((sx + 3) * (sx + 3) + (sy + 1) * (sy + 1) <= r4_sq) cov++;
                if ((sx + 1) * (sx + 1) + (sy + 3) * (sy + 3) <= r4_sq) cov++;
                if ((sx + 3) * (sx + 3) + (sy + 3) * (sy + 3) <= r4_sq) cov++;
                if (cov == 0) continue;

                int eff_alpha = (alpha * cov) / 4;
                glass_blend(bb, stride, px, py, c, eff_alpha, scr_w, scr_h);
            }
        }
    }
}

static void glass_round_outline_aa(uint32_t *bb, int stride, const rect_t *r,
                                   int radius, color_t c, int thickness, int alpha,
                                   int scr_w, int scr_h) {
    if (r->w <= 0 || r->h <= 0 || thickness <= 0 || alpha <= 0) return;
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;

    rect_t top = { r->x + radius, r->y, r->w - radius * 2, thickness };
    glass_fill_rect_aa(bb, stride, &top, c, alpha, scr_w, scr_h);
    rect_t bot = { r->x + radius, r->y + r->h - thickness, r->w - radius * 2, thickness };
    glass_fill_rect_aa(bb, stride, &bot, c, alpha, scr_w, scr_h);
    rect_t lft = { r->x, r->y + radius, thickness, r->h - radius * 2 };
    glass_fill_rect_aa(bb, stride, &lft, c, alpha, scr_w, scr_h);
    rect_t rgt = { r->x + r->w - thickness, r->y + radius, thickness, r->h - radius * 2 };
    glass_fill_rect_aa(bb, stride, &rgt, c, alpha, scr_w, scr_h);

    int ro = radius * radius;
    int inner = radius - thickness;
    int ri = inner > 0 ? inner * inner : 0;
    for (int q = 0; q < 4; q++) {
        int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
        int cx = (q & 1) ? (r->x + r->w - radius - 1) : (r->x + radius);
        int cy = (q & 2) ? (r->y + r->h - radius - 1) : (r->y + radius);
        for (int dy = 0; dy <= radius; dy++) {
            for (int dx = 0; dx <= radius; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 > ro || d2 < ri) continue;
                for (int t = 0; t < thickness; t++) {
                    int px = cx + sx * (dx - t);
                    int py = cy + sy * (dy - t);
                    glass_blend(bb, stride, px, py, c, alpha, scr_w, scr_h);
                }
            }
        }
    }
}

/* =========================================================================
 * Fast Backdrop Frosted-Glass Blur
 * ========================================================================= */

void glass_backdrop_blur(uint32_t *bb, int stride, const rect_t *r, int radius) {
    if (!MyOSTheme.blur_enabled || MyOSTheme.reduced_effects || !bb || !r) return;
    if (r->w <= 4 || r->h <= 4 || radius <= 0) return;

    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;

    int x0 = r->x < 0 ? 0 : r->x;
    int y0 = r->y < 0 ? 0 : r->y;
    int x1 = r->x + r->w > scr_w ? scr_w : r->x + r->w;
    int y1 = r->y + r->h > scr_h ? scr_h : r->y + r->h;
    if (x0 >= x1 || y0 >= y1) return;

    /* High-speed frosted blur kernel on backdrop pixels */
    for (int y = y0; y < y1; y += 2) {
        uint32_t *row0 = bb + (size_t)y * (size_t)stride;
        int next_y = (y + 1 < y1) ? y + 1 : y;
        uint32_t *row1 = bb + (size_t)next_y * (size_t)stride;

        for (int x = x0 + 1; x < x1 - 1; x += 2) {
            uint32_t c00 = row0[x - 1];
            uint32_t c01 = row0[x];
            uint32_t c02 = row0[x + 1];
            uint32_t c10 = row1[x - 1];
            uint32_t c11 = row1[x];
            uint32_t c12 = row1[x + 1];

            uint32_t sum_r = (((c00 >> 16) & 0xFF) + ((c01 >> 16) & 0xFF) + ((c02 >> 16) & 0xFF) +
                              ((c10 >> 16) & 0xFF) + ((c11 >> 16) & 0xFF) + ((c12 >> 16) & 0xFF)) / 6;
            uint32_t sum_g = (((c00 >> 8) & 0xFF) + ((c01 >> 8) & 0xFF) + ((c02 >> 8) & 0xFF) +
                              ((c10 >> 8) & 0xFF) + ((c11 >> 8) & 0xFF) + ((c12 >> 8) & 0xFF)) / 6;
            uint32_t sum_b = ((c00 & 0xFF) + (c01 & 0xFF) + (c02 & 0xFF) +
                              (c10 & 0xFF) + (c11 & 0xFF) + (c12 & 0xFF)) / 6;

            uint32_t blurred = (sum_r << 16) | (sum_g << 8) | sum_b;
            row0[x] = blurred;
            if (x + 1 < x1) row0[x + 1] = blurred;
            row1[x] = blurred;
            if (x + 1 < x1) row1[x + 1] = blurred;
        }
    }
}

/* =========================================================================
 * Reusable Glass Primitives on Backbuffer
 * ========================================================================= */

void glass_render_surface(uint32_t *bb, int stride, const rect_t *r,
                          int radius, color_t tint, uint8_t alpha,
                          bool with_blur, bool with_shadow, bool with_specular) {
    if (!bb || !r || r->w <= 0 || r->h <= 0) return;

    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;

    /* 1. Backdrop Frosted Blur Filter */
    if (with_blur && MyOSTheme.blur_enabled && !MyOSTheme.reduced_effects) {
        glass_backdrop_blur(bb, stride, r, MyOSTheme.blur_radius);
    }

    /* 2. Soft Ambient Drop Shadow */
    if (with_shadow && !MyOSTheme.reduced_effects) {
        rect_t sh1 = { r->x + 2, r->y + 4, r->w, r->h };
        glass_round_rect_aa(bb, stride, &sh1, radius, RGB(0, 0, 0), 105, scr_w, scr_h);
        rect_t sh2 = { r->x + 4, r->y + 8, r->w - 4, r->h - 2 };
        glass_round_rect_aa(bb, stride, &sh2, radius > 2 ? radius - 2 : radius, RGB(0, 0, 0), 65, scr_w, scr_h);
    }

    /* 3. Translucent Frosted Glass Body Tint */
    glass_round_rect_aa(bb, stride, r, radius, tint, alpha, scr_w, scr_h);

    /* 4. Subtle 10-15% White Glass Border */
    glass_round_outline_aa(bb, stride, r, radius, MyOSTheme.border_glass, 1,
                           MyOSTheme.alpha_border, scr_w, scr_h);

    /* 5. Restrained Upper-Edge Specular Highlight */
    if (with_specular) {
        int rad_off = radius > 4 ? radius : 4;
        rect_t shine = { r->x + rad_off, r->y + 1, r->w - rad_off * 2, 1 };
        glass_fill_rect_aa(bb, stride, &shine, MyOSTheme.highlight_shine,
                           MyOSTheme.alpha_shine, scr_w, scr_h);
    }
}

void glass_render_panel(uint32_t *bb, int stride, const rect_t *r) {
    glass_render_surface(bb, stride, r, MyOSTheme.radius_panel,
                         MyOSTheme.glass_surface_primary, MyOSTheme.alpha_panel,
                         true, true, true);
}

void glass_render_button(uint32_t *bb, int stride, const rect_t *r,
                         const char *label, const baked_font_t *font,
                         bool hover, bool active, color_t text_color) {
    if (!bb || !r || r->w <= 0 || r->h <= 0) return;
    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;

    color_t tint = active ? MyOSTheme.accent_primary :
                   (hover ? MyOSTheme.glass_surface_sec : MyOSTheme.glass_surface_primary);
    uint8_t alpha = active ? 230 : (hover ? MyOSTheme.alpha_button_hover : 160);

    glass_render_surface(bb, stride, r, MyOSTheme.radius_button, tint, alpha,
                         false, hover, true);

    if (hover) {
        glass_round_outline_aa(bb, stride, r, MyOSTheme.radius_button,
                               MyOSTheme.accent_primary, 1, 190, scr_w, scr_h);
    }

    if (label && font) {
        int tw = text_width(font, label);
        int tx = r->x + (r->w - tw) / 2;
        int ty = r->y + (r->h - 14) / 2;
        /* Draw text */
        for (int i = 0; label[i]; ) {
            uint32_t cp;
            int n = utf8_decode(label + i, &cp);
            i += n;
            if (cp < (uint32_t)font->first_cp || cp >= (uint32_t)(font->first_cp + font->glyph_count))
                continue;
            int idx = (int)cp - font->first_cp;
            int gw = font->gw[idx], gh = font->gh[idx];
            int xo = font->xo[idx], yo = font->yo[idx];
            int xa = font->xa[idx];
            const uint8_t *bits = font->bitmap + font->bo[idx];
            int pitch_bits = (gw + 7) / 8;
            for (int row = 0; row < gh; row++) {
                int py = ty + yo + row;
                if (py < 0 || py >= scr_h) continue;
                for (int col = 0; col < gw; col++) {
                    int px = tx + xo + col;
                    if (px < 0 || px >= scr_w) continue;
                    if (bits[row * pitch_bits + (col >> 3)] & (1 << (col & 7))) {
                        bb[(size_t)py * (size_t)stride + px] = text_color;
                    }
                }
            }
            tx += xa;
        }
    }
}

void glass_render_notification(uint32_t *bb, int stride,
                               int x, int y, int w, int h,
                               const char *title, const char *msg,
                               color_t accent) {
    rect_t r = { x, y, w, h };
    glass_render_surface(bb, stride, &r, MyOSTheme.radius_card,
                         MyOSTheme.glass_surface_primary, MyOSTheme.alpha_notification,
                         true, true, true);

    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;

    /* Accent Pill Badge */
    rect_t badge = { x + 12, y + 12, 6, h - 24 };
    glass_round_rect_aa(bb, stride, &badge, 3, accent, 240, scr_w, scr_h);

    /* Text */
    if (title) {
        int tx = x + 26, ty = y + 10;
        const baked_font_t *font = font_bold();
        for (int i = 0; title[i]; ) {
            uint32_t cp;
            int n = utf8_decode(title + i, &cp);
            i += n;
            if (cp < (uint32_t)font->first_cp || cp >= (uint32_t)(font->first_cp + font->glyph_count)) continue;
            int idx = (int)cp - font->first_cp;
            int gw = font->gw[idx], gh = font->gh[idx], xo = font->xo[idx], yo = font->yo[idx], xa = font->xa[idx];
            const uint8_t *bits = font->bitmap + font->bo[idx];
            int pitch_bits = (gw + 7) / 8;
            for (int row = 0; row < gh; row++) {
                int py = ty + yo + row;
                if (py < 0 || py >= scr_h) continue;
                for (int col = 0; col < gw; col++) {
                    int px = tx + xo + col;
                    if (px < 0 || px >= scr_w) continue;
                    if (bits[row * pitch_bits + (col >> 3)] & (1 << (col & 7)))
                        bb[(size_t)py * (size_t)stride + px] = MyOSTheme.text_primary;
                }
            }
            tx += xa;
        }
    }
    if (msg) {
        int tx = x + 26, ty = y + 28;
        const baked_font_t *font = font_ui();
        for (int i = 0; msg[i]; ) {
            uint32_t cp;
            int n = utf8_decode(msg + i, &cp);
            i += n;
            if (cp < (uint32_t)font->first_cp || cp >= (uint32_t)(font->first_cp + font->glyph_count)) continue;
            int idx = (int)cp - font->first_cp;
            int gw = font->gw[idx], gh = font->gh[idx], xo = font->xo[idx], yo = font->yo[idx], xa = font->xa[idx];
            const uint8_t *bits = font->bitmap + font->bo[idx];
            int pitch_bits = (gw + 7) / 8;
            for (int row = 0; row < gh; row++) {
                int py = ty + yo + row;
                if (py < 0 || py >= scr_h) continue;
                for (int col = 0; col < gw; col++) {
                    int px = tx + xo + col;
                    if (px < 0 || px >= scr_w) continue;
                    if (bits[row * pitch_bits + (col >> 3)] & (1 << (col & 7)))
                        bb[(size_t)py * (size_t)stride + px] = MyOSTheme.text_secondary;
                }
            }
            tx += xa;
        }
    }
}

void glass_render_tooltip(uint32_t *bb, int stride,
                          int x, int y, const char *text) {
    if (!text || !bb) return;
    int tw = text_width(font_ui(), text);
    rect_t r = { x, y, tw + 16, 22 };
    glass_render_surface(bb, stride, &r, MyOSTheme.radius_input,
                         MyOSTheme.glass_surface_sec, 225, false, true, true);
    /* Draw text */
    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;
    int tx = x + 8, ty = y + 4;
    const baked_font_t *font = font_ui();
    for (int i = 0; text[i]; ) {
        uint32_t cp;
        int n = utf8_decode(text + i, &cp);
        i += n;
        if (cp < (uint32_t)font->first_cp || cp >= (uint32_t)(font->first_cp + font->glyph_count)) continue;
        int idx = (int)cp - font->first_cp;
        int gw = font->gw[idx], gh = font->gh[idx], xo = font->xo[idx], yo = font->yo[idx], xa = font->xa[idx];
        const uint8_t *bits = font->bitmap + font->bo[idx];
        int pitch_bits = (gw + 7) / 8;
        for (int row = 0; row < gh; row++) {
            int py = ty + yo + row;
            if (py < 0 || py >= scr_h) continue;
            for (int col = 0; col < gw; col++) {
                int px = tx + xo + col;
                if (px < 0 || px >= scr_w) continue;
                if (bits[row * pitch_bits + (col >> 3)] & (1 << (col & 7)))
                    bb[(size_t)py * (size_t)stride + px] = MyOSTheme.text_primary;
            }
        }
        tx += xa;
    }
}

/* =========================================================================
 * Reusable Glass Primitives on Surface
 * ========================================================================= */

void glass_surface_card(surface_t *s, const rect_t *area, const char *title) {
    if (!s || !area || area->w <= 0 || area->h <= 0) return;

    /* Elevated frosted glass card body */
    surface_rounded_fill(s, area, MyOSTheme.radius_card, MyOSTheme.glass_surface_primary);
    surface_rounded_outline(s, area, MyOSTheme.radius_card, RGB(0x38, 0x4B, 0x6E), 1);

    /* Upper edge specular glass highlight */
    rect_t shine = { area->x + 8, area->y + 1, area->w - 16, 1 };
    surface_fill_rect(s, &shine, RGB(0x9E, 0xC4, 0xFA));

    /* Frosted Header Ribbon */
    if (title) {
        rect_t hdr = { area->x + 1, area->y + 1, area->w - 2, 28 };
        surface_rounded_fill(s, &hdr, MyOSTheme.radius_card - 1, MyOSTheme.glass_surface_sec);
        rect_t hdr_line = { area->x + 1, area->y + 28, area->w - 2, 1 };
        surface_fill_rect(s, &hdr_line, RGB(0x32, 0x44, 0x66));
        text_draw(s, font_bold(), area->x + 12, area->y + 7, title, MyOSTheme.text_primary);
    }
}

void glass_surface_button(surface_t *s, const rect_t *r,
                          const char *label, const baked_font_t *font,
                          bool hover, bool active, color_t text_color) {
    if (!s || !r || r->w <= 0 || r->h <= 0) return;

    color_t bg = active ? MyOSTheme.accent_primary :
                 (hover ? MyOSTheme.glass_surface_sec : MyOSTheme.glass_surface_primary);
    color_t border = hover ? MyOSTheme.accent_primary : RGB(0x3E, 0x54, 0x7E);

    surface_rounded_fill(s, r, MyOSTheme.radius_button, bg);
    surface_rounded_outline(s, r, MyOSTheme.radius_button, border, 1);

    rect_t shine = { r->x + 4, r->y + 1, r->w - 8, 1 };
    surface_fill_rect(s, &shine, RGB(0xA0, 0xC8, 0xFF));

    if (label && font) {
        int tw = text_width(font, label);
        int tx = r->x + (r->w - tw) / 2;
        int ty = r->y + (r->h - 14) / 2;
        text_draw(s, font, tx, ty, label, text_color);
    }
}

void glass_surface_textfield(surface_t *s, const rect_t *r,
                             const char *text, const char *placeholder,
                             bool focused) {
    if (!s || !r || r->w <= 0 || r->h <= 0) return;

    surface_rounded_fill(s, r, MyOSTheme.radius_input, RGB(0x0E, 0x14, 0x22));
    surface_rounded_outline(s, r, MyOSTheme.radius_input,
                            focused ? MyOSTheme.accent_primary : RGB(0x32, 0x46, 0x66), 1);

    if (focused) {
        rect_t shine = { r->x + 4, r->y + 1, r->w - 8, 1 };
        surface_fill_rect(s, &shine, RGB(0x65, 0xD9, 0xFF));
    }

    if (text && text[0]) {
        text_draw(s, font_ui(), r->x + 8, r->y + (r->h - 14) / 2, text, MyOSTheme.text_primary);
    } else if (placeholder) {
        text_draw(s, font_ui(), r->x + 8, r->y + (r->h - 14) / 2, placeholder, MyOSTheme.text_dim);
    }
}
