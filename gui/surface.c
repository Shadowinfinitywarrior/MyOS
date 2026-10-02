#include "surface.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "../drivers/framebuffer.h"

surface_t *surface_create(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;
    surface_t *s = kzalloc(sizeof(surface_t));
    if (!s) return NULL;
    s->pixels = kmalloc((size_t)w * h * 4);
    if (!s->pixels) { kfree(s); return NULL; }
    s->w = w;
    s->h = h;
    s->pitch = w * 4;
    s->full_damage = true;
    return s;
}

void surface_destroy(surface_t *s) {
    if (!s) return;
    if (s->pixels) kfree(s->pixels);
    kfree(s);
}

void surface_clear(surface_t *s, color_t c) {
    if (!s || !s->pixels) return;
    rect_t all = { 0, 0, s->w, s->h };
    blit_fill(s->pixels, s->pitch, &all, c);
    surface_damage_all(s);
}

void surface_damage_all(surface_t *s) {
    if (!s) return;
    s->full_damage = true;
    s->damage_count = 0;
}

void surface_clear_damage(surface_t *s) {
    if (!s) return;
    s->full_damage = false;
    s->damage_count = 0;
}

void surface_damage(surface_t *s, const rect_t *r) {
    if (!s || rect_is_empty(r)) return;
    if (s->full_damage) return;
    rect_t c = *r;
    if (c.x < 0) { c.w += c.x; c.x = 0; }
    if (c.y < 0) { c.h += c.y; c.y = 0; }
    if (c.x + c.w > s->w) c.w = s->w - c.x;
    if (c.y + c.h > s->h) c.h = s->h - c.y;
    if (c.w <= 0 || c.h <= 0) return;
    if (s->damage_count < (int)ARRAY_SIZE(s->damage)) {
        s->damage[s->damage_count++] = c;
    } else {
        s->full_damage = true;
        s->damage_count = 0;
    }
}

bool surface_damage_rect(surface_t *s, rect_t *out) {
    if (!s) return false;
    if (s->full_damage) {
        if (out) { out->x = 0; out->y = 0; out->w = s->w; out->h = s->h; }
        return true;
    }
    if (s->damage_count == 0) return false;
    if (out) *out = s->damage[0];
    for (int i = 1; i < s->damage_count; i++) {
        rect_union(out, &s->damage[i], out);
    }
    return true;
}

void surface_pixel(surface_t *s, int x, int y, color_t c) {
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    ((uint32_t *)((uint8_t *)s->pixels + (size_t)y * s->pitch))[x] = c;
}

void surface_fill_rect(surface_t *s, const rect_t *r, color_t c) {
    if (!s) return;
    blit_fill(s->pixels, s->pitch, r, c);
    surface_damage(s, r);
}

void surface_gradient_v(surface_t *s, const rect_t *r, color_t top, color_t bottom) {
    if (!s) return;
    blit_vertical_gradient(s->pixels, s->pitch, r, top, bottom);
    surface_damage(s, r);
}

void surface_horizontal_gradient(surface_t *s, const rect_t *r, color_t left, color_t right) {
    if (!s || rect_is_empty(r)) return;
    for (int y = 0; y < r->h; y++) {
        for (int x = 0; x < r->w; x++) {
            uint32_t t = r->w > 1 ? (uint32_t)((uint64_t)x * 255 / (uint64_t)(r->w - 1)) : 0;
            uint32_t sr = (left >> 16) & 0xFF, sg = (left >> 8) & 0xFF, sb = left & 0xFF;
            uint32_t er = (right >> 16) & 0xFF, eg = (right >> 8) & 0xFF, eb = right & 0xFF;
            uint32_t rr = sr + (((er - sr) * t) >> 8);
            uint32_t gg = sg + (((eg - sg) * t) >> 8);
            uint32_t bb = sb + (((eb - sb) * t) >> 8);
            surface_pixel(s, r->x + x, r->y + y, (rr << 16) | (gg << 8) | bb);
        }
    }
    surface_damage(s, r);
}

void surface_rect_outline(surface_t *s, const rect_t *r, color_t c, int thickness) {
    if (!s || rect_is_empty(r) || thickness <= 0) return;
    rect_t top    = { r->x, r->y, r->w, thickness };
    rect_t bottom = { r->x, r->y + r->h - thickness, r->w, thickness };
    rect_t left   = { r->x, r->y + thickness, thickness, r->h - thickness * 2 };
    rect_t right  = { r->x + r->w - thickness, r->y + thickness, thickness, r->h - thickness * 2 };
    surface_fill_rect(s, &top, c);
    surface_fill_rect(s, &bottom, c);
    surface_fill_rect(s, &left, c);
    surface_fill_rect(s, &right, c);
}

/* Rounded corners via a quarter-disc test per pixel in the corner boxes only;
 * the body of the rect is a plain fill. */
void surface_rounded_fill(surface_t *s, const rect_t *r, int radius, color_t c) {
    if (!s || rect_is_empty(r)) return;
    if (radius <= 0) { surface_fill_rect(s, r, c); return; }
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;

    rect_t body = { r->x, r->y + radius, r->w, r->h - radius * 2 };
    surface_fill_rect(s, &body, c);
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, radius };
    rect_t bot = { r->x + radius, r->y + r->h - radius, r->w - radius * 2, radius };
    surface_fill_rect(s, &top, c);
    surface_fill_rect(s, &bot, c);

    int rr = radius * radius;
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
                int px = (k & 1) ? (cx + radius - 1 - dx) : (cx + dx);
                int py = (k & 2) ? (cy + radius - 1 - dy) : (cy + dy);
                int ex = px - cx, ey = py - cy;
                if (ex * ex + ey * ey <= rr) surface_pixel(s, px, py, c);
            }
        }
    }
}

void surface_rounded_outline(surface_t *s, const rect_t *r, int radius, color_t c,
                             int thickness) {
    if (!s || rect_is_empty(r) || thickness <= 0) return;
    if (radius <= 0) { surface_rect_outline(s, r, c, thickness); return; }
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;

    /* Top and bottom straight runs between the corner arcs. */
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, thickness };
    rect_t bot = { r->x + radius, r->y + r->h - thickness, r->w - radius * 2, thickness };
    surface_fill_rect(s, &top, c);
    surface_fill_rect(s, &bot, c);

    int ro = radius * radius;
    int inner = radius - thickness;
    int ri = inner > 0 ? inner * inner : 0;

    /* Corner arcs: keep pixels whose distance from the arc centre falls in the
     * [inner,outer] band, then extrude each one `thickness` steps toward the
     * rect's centre so the ring has real width. */
    for (int q = 0; q < 4; q++) {
        int sx = (q & 1) ? -1 : 1;      /* outward direction on x */
        int sy = (q & 2) ? -1 : 1;      /* outward direction on y */
        int cx = (q & 1) ? (r->x + r->w - radius - 1) : (r->x + radius);
        int cy = (q & 2) ? (r->y + r->h - radius - 1) : (r->y + radius);
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                int ox = sx * dx, oy = sy * dy;
                int d2 = ox * ox + oy * oy;
                if (d2 > ro || d2 < ri) continue;
                int px = cx + ox, py = cy + oy;
                for (int t = 0; t < thickness; t++) {
                    int qx = px - sx * t, qy = py - sy * t;
                    if (qx >= r->x && qx < r->x + r->w &&
                        qy >= r->y && qy < r->y + r->h)
                        surface_pixel(s, qx, qy, c);
                }
            }
        }
    }
}

void surface_blit_surface(surface_t *dst, const surface_t *src, int x, int y) {
    if (!dst || !src) return;
    rect_t r = { x, y, src->w, src->h };
    rect_t clip = { 0, 0, dst->w, dst->h };
    rect_t vis;
    if (!rect_intersect(&r, &clip, &vis)) return;
    blit_copy_offset(dst->pixels, dst->pitch, vis.x, vis.y,
                     src->pixels, src->pitch, vis.x - x, vis.y - y, vis.w, vis.h);
    surface_damage(dst, &vis);
}

void surface_present(surface_t *s, int dx, int dy, const rect_t *src) {
    uint32_t *bb = fb_get_backbuffer();
    if (!bb || !s) return;
    rect_t clip = { 0, 0, fb_get_info()->width, fb_get_info()->height };
    rect_t dest = { dx + (src ? src->x : 0), dy + (src ? src->y : 0),
                    src ? src->w : s->w, src ? src->h : s->h };
    rect_t vis;
    if (!rect_intersect(&dest, &clip, &vis)) return;
    /* Source origin is the visible rect shifted back by the dest offset. */
    int sx = (src ? src->x : 0) + (vis.x - dest.x);
    int sy = (src ? src->y : 0) + (vis.y - dest.y);
    blit_copy_offset(bb, fb_get_stride() * 4, vis.x, vis.y,
                     s->pixels, s->pitch, sx, sy, vis.w, vis.h);
    fb_add_damage(vis.x, vis.y, vis.w, vis.h);
}
