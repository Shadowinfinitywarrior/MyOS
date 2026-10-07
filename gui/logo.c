#include "logo.h"
#include "theme.h"
#include "text.h"
#include "../lib/string.h"

/* Fast integer square root */
static uint32_t isqrt(uint32_t val) {
    uint32_t res = 0;
    uint32_t bit = 1u << 30;
    while (bit > val) bit >>= 2;
    while (bit != 0) {
        if (val >= res + bit) {
            val -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

static inline void blend_px(uint32_t *bb, int stride, int x, int y,
                            int scr_w, int scr_h, color_t c, uint32_t alpha) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h || alpha == 0) return;
    if (alpha >= 255) {
        bb[(size_t)y * (size_t)stride + x] = c;
    } else {
        uint32_t d = bb[(size_t)y * (size_t)stride + x];
        uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
        uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
        uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
        bb[(size_t)y * (size_t)stride + x] = (r0 << 16) | (g0 << 8) | b0;
    }
}

static void draw_bb_text(uint32_t *bb, int stride, const baked_font_t *f, int x, int y,
                         int scr_w, int scr_h, const char *str, color_t c) {
    if (!bb || !f || !str) return;
    int pen = x;
    for (int i = 0; str[i]; ) {
        uint32_t cp;
        int n = utf8_decode(str + i, &cp);
        i += n;
        if (cp < (uint32_t)f->first_cp || cp >= (uint32_t)(f->first_cp + f->glyph_count))
            continue;
        int idx = (int)cp - f->first_cp;
        int gw = f->gw[idx], gh = f->gh[idx];
        int xo = f->xo[idx], yo = f->yo[idx];
        int xa = f->xa[idx];
        const uint8_t *bits = f->bitmap + f->bo[idx];
        int pitch_bits = (gw + 7) / 8;
        for (int r = 0; r < gh; r++) {
            int py = y + yo + r;
            if (py < 0 || py >= scr_h) continue;
            for (int col = 0; col < gw; col++) {
                int px_x = pen + xo + col;
                if (px_x < 0 || px_x >= scr_w) continue;
                if (bits[r * pitch_bits + (col >> 3)] & (1 << (col & 7))) {
                    blend_px(bb, stride, px_x, py, scr_w, scr_h, c, 255);
                }
            }
        }
        pen += xa;
    }
}

/* Linear interpolate color */
static color_t lerp_color(color_t c1, color_t c2, int t256) {
    if (t256 <= 0) return c1;
    if (t256 >= 256) return c2;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint32_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint32_t r = r1 + ((r2 - r1) * (uint32_t)t256) / 256;
    uint32_t g = g1 + ((g2 - g1) * (uint32_t)t256) / 256;
    uint32_t b = b1 + ((b2 - b1) * (uint32_t)t256) / 256;
    return (r << 16) | (g << 8) | b;
}

/* Draw a glowing filled circle with soft falloff */
static void draw_glow_circle(uint32_t *bb, int stride, int scr_w, int scr_h,
                             int cx, int cy, int radius, color_t core_c, color_t glow_c) {
    int r_max = radius + (radius / 2) + 1;
    int r_sq = radius * radius;
    for (int dy = -r_max; dy <= r_max; dy++) {
        for (int dx = -r_max; dx <= r_max; dx++) {
            int d2 = dx * dx + dy * dy;
            int px = cx + dx, py = cy + dy;
            if (d2 <= r_sq) {
                int t = 256 - (d2 * 256) / (r_sq ? r_sq : 1);
                color_t col = lerp_color(glow_c, core_c, t);
                blend_px(bb, stride, px, py, scr_w, scr_h, col, 255);
            } else {
                int max2 = r_max * r_max;
                if (d2 < max2) {
                    int a = 180 - ((d2 - r_sq) * 180) / (max2 - r_sq);
                    if (a > 0) blend_px(bb, stride, px, py, scr_w, scr_h, glow_c, (uint32_t)a);
                }
            }
        }
    }
}

/* Minimum distance squared from point (px, py) to line segment (x1, y1)-(x2, y2) */
static int dist_sq_segment(int px, int py, int x1, int y1, int x2, int y2) {
    int l2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
    if (l2 == 0) return (px - x1) * (px - x1) + (py - y1) * (py - y1);
    int t = ((px - x1) * (x2 - x1) + (py - y1) * (y2 - y1));
    if (t < 0) return (px - x1) * (px - x1) + (py - y1) * (py - y1);
    if (t > l2) return (px - x2) * (px - x2) + (py - y2) * (py - y2);
    int proj_x = x1 + (t * (x2 - x1)) / l2;
    int proj_y = y1 + (t * (y2 - y1)) / l2;
    return (px - proj_x) * (px - proj_x) + (py - proj_y) * (py - proj_y);
}

/* Render MyOS planetary orbital ribbon 'M' logo */
void logo_draw_buffer(uint32_t *bb, int stride, int scr_w, int scr_h,
                      int cx, int cy, int size) {
    if (!bb || size <= 4) return;

    int half = size / 2;
    int rad = size * 45 / 100;
    int ribbon_w = size * 16 / 100;
    if (ribbon_w < 2) ribbon_w = 2;
    int r_half = ribbon_w / 2;

    /* Palette */
    color_t c_cyan   = RGB(0x00, 0xE5, 0xFF); /* Electric Cyan */
    color_t c_blue   = RGB(0x2D, 0x68, 0xFF); /* Vibrant Royal Blue */
    color_t c_violet = RGB(0x8B, 0x5C, 0xF6); /* Purple/Violet */
    color_t c_orchid = RGB(0xD9, 0x46, 0xEF); /* Neon Orchid */
    color_t c_pink   = RGB(0xF4, 0x3F, 0x5E); /* Rose Magenta */
    (void)c_violet;

    /* Key skeletal anchor points of the 'M' ribbon */
    int p0_x = cx - size * 30 / 100, p0_y = cy + size * 30 / 100; /* Bottom left */
    int p1_x = cx - size * 22 / 100, p1_y = cy - size * 26 / 100; /* Top left peak */
    int p2_x = cx,                   p2_y = cy + size * 10 / 100; /* Center dip */
    int p3_x = cx + size * 22 / 100, p3_y = cy - size * 26 / 100; /* Top right peak */
    int p4_x = cx + size * 30 / 100, p4_y = cy + size * 30 / 100; /* Bottom right */

    /* Intermediate bezier-like points for curvature */
    int p01_x = cx - size * 31 / 100, p01_y = cy;
    int p12_x = cx - size * 10 / 100, p12_y = cy - size * 6 / 100;
    int p23_x = cx + size * 10 / 100, p23_y = cy - size * 6 / 100;
    int p34_x = cx + size * 31 / 100, p34_y = cy;

    /* 1. Draw Planetary Orbital Ellipse Ring (Tilted) */
    /* Rotation: cos ~ 0.92, sin ~ -0.38 (approx -22.5 deg) */
    int cos_q8 = 236, sin_q8 = -98; /* / 256 */
    int a_axis = size * 54 / 100;
    int b_axis = size * 19 / 100;
    if (a_axis < 4) a_axis = 4;
    if (b_axis < 2) b_axis = 2;

    int scan_rad = half + size * 18 / 100;
    for (int dy = -scan_rad; dy <= scan_rad; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= scr_h) continue;
        for (int dx = -scan_rad; dx <= scan_rad; dx++) {
            int x = cx + dx;
            if (x < 0 || x >= scr_w) continue;

            /* Rotated ellipse coordinate */
            int rx = (dx * cos_q8 - dy * sin_q8) / 256;
            int ry = (dx * sin_q8 + dy * cos_q8) / 256;

            /* Approximate ellipse distance */
            int ex = (rx * 100) / a_axis;
            int ey = (ry * 100) / b_axis;
            int ell_d2 = ex * ex + ey * ey;
            int ell_dist = (int)isqrt((uint32_t)ell_d2);
            int delta = ell_dist - 100;
            if (delta < 0) delta = -delta;

            /* Ring thickness 1.5 - 2.5 px */
            if (delta <= 12) {
                int ring_a = 240 - delta * 18;
                if (ring_a > 0) {
                    /* Gradient along orbit: cyan to electric sapphire */
                    int t_orb = (dx + scan_rad) * 256 / (scan_rad * 2 + 1);
                    color_t orb_c = lerp_color(c_cyan, c_blue, t_orb);
                    blend_px(bb, stride, x, y, scr_w, scr_h, orb_c, (uint32_t)(ring_a * 85 / 100));
                }
            } else if (delta <= 24) {
                /* Ambient outer halo glow of orbital ring */
                int glow_a = (24 - delta) * 4;
                if (glow_a > 0) {
                    blend_px(bb, stride, x, y, scr_w, scr_h, c_cyan, (uint32_t)glow_a);
                }
            }
        }
    }

    /* 2. Draw 3D Ribbon 'M' Body with Antialiasing */
    for (int dy = -rad - 2; dy <= rad + 2; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= scr_h) continue;
        for (int dx = -rad - 2; dx <= rad + 2; dx++) {
            int x = cx + dx;
            if (x < 0 || x >= scr_w) continue;

            /* Measure distance to skeleton strokes */
            int d0 = dist_sq_segment(x, y, p0_x, p0_y, p01_x, p01_y);
            int d1 = dist_sq_segment(x, y, p01_x, p01_y, p1_x, p1_y);
            int d2 = dist_sq_segment(x, y, p1_x, p1_y, p12_x, p12_y);
            int d3 = dist_sq_segment(x, y, p12_x, p12_y, p2_x, p2_y);
            int d4 = dist_sq_segment(x, y, p2_x, p2_y, p23_x, p23_y);
            int d5 = dist_sq_segment(x, y, p23_x, p23_y, p3_x, p3_y);
            int d6 = dist_sq_segment(x, y, p3_x, p3_y, p34_x, p34_y);
            int d7 = dist_sq_segment(x, y, p34_x, p34_y, p4_x, p4_y);

            int min_d2 = d0;
            if (d1 < min_d2) min_d2 = d1;
            if (d2 < min_d2) min_d2 = d2;
            if (d3 < min_d2) min_d2 = d3;
            if (d4 < min_d2) min_d2 = d4;
            if (d5 < min_d2) min_d2 = d5;
            if (d6 < min_d2) min_d2 = d6;
            if (d7 < min_d2) min_d2 = d7;

            int dist = (int)isqrt((uint32_t)min_d2);

            /* Check stroke coverage */
            if (dist <= r_half + 1) {
                int cov_alpha = 255;
                if (dist > r_half - 1) {
                    /* Antialiased edge transition */
                    cov_alpha = (r_half + 1 - dist) * 128;
                    if (cov_alpha > 255) cov_alpha = 255;
                    if (cov_alpha < 0) cov_alpha = 0;
                }

                /* Compute horizontal progression t along the ribbon for color */
                int span = p4_x - p0_x;
                int progress = span ? ((x - p0_x) * 256 / span) : 128;
                if (progress < 0) progress = 0;
                if (progress > 256) progress = 256;

                color_t base_c;
                if (progress < 85) {
                    base_c = lerp_color(c_cyan, c_blue, (progress * 256) / 85);
                } else if (progress < 170) {
                    base_c = lerp_color(c_blue, c_orchid, ((progress - 85) * 256) / 85);
                } else {
                    base_c = lerp_color(c_orchid, c_pink, ((progress - 170) * 256) / 86);
                }

                /* 3D cylindrical specular shading: highlight along upper crest */
                int crest_y = cy - (dist * 2 / 3);
                int spec = 0;
                if (y < crest_y + 2 && y > crest_y - 4) {
                    spec = 35;
                }

                uint32_t r = ((base_c >> 16) & 0xFF) + spec;
                uint32_t g = ((base_c >> 8) & 0xFF) + spec;
                uint32_t b = (base_c & 0xFF) + spec;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                color_t shaded = (r << 16) | (g << 8) | b;

                blend_px(bb, stride, x, y, scr_w, scr_h, shaded, (uint32_t)cov_alpha);
            }
        }
    }

    /* 3. Glowing Celestial Satellite Bead on Upper-Right Orbit */
    /* Position at ~ 35 degrees on rotated orbit */
    int sat_x = cx + (a_axis * 82 / 100);
    int sat_y = cy - (b_axis * 50 / 100);
    int sat_r = size * 6 / 100;
    if (sat_r < 2) sat_r = 2;
    draw_glow_circle(bb, stride, scr_w, scr_h, sat_x, sat_y, sat_r, RGB(0xFF, 0xFF, 0xFF), c_cyan);
}

/* Draw to surface_t wrapper */
void logo_draw_surface(surface_t *s, int cx, int cy, int size) {
    if (!s) return;
    logo_draw_buffer(s->pixels, s->pitch / 4, s->w, s->h, cx, cy, size);
}

/* Draw full brand badge with MyOS text and subtitle */
void logo_draw_badge(uint32_t *bb, int stride, int scr_w, int scr_h,
                     int x, int y, int size, bool with_text, bool with_subtitle) {
    if (!bb) return;
    int logo_cx = x + size / 2;
    int logo_cy = y + size / 2;
    logo_draw_buffer(bb, stride, scr_w, scr_h, logo_cx, logo_cy, size);

    if (with_text) {
        int tx = x + size + size * 20 / 100;
        int ty = y + (size / 2) - 10;

        /* "My" in pure crisp white */
        draw_bb_text(bb, stride, font_bold(), tx, ty, scr_w, scr_h, "My", RGB(0xFF, 0xFF, 0xFF));
        int my_w = text_width(font_bold(), "My");

        /* "OS" in signature gradient cyan/violet */
        draw_bb_text(bb, stride, font_bold(), tx + my_w, ty, scr_w, scr_h, "OS", RGB(0x00, 0xD2, 0xFF));

        if (with_subtitle) {
            draw_bb_text(bb, stride, font_ui(), tx, ty + 18, scr_w, scr_h,
                         "FREEDOM • PRIVACY • PERFORMANCE", RGB(0x8A, 0x9B, 0xB5));
        }
    }
}
