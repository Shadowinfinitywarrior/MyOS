#include "rect.h"

bool rect_intersect(const rect_t *a, const rect_t *b, rect_t *out) {
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    if (x1 <= x0 || y1 <= y0) {
        if (out) { out->x = x0; out->y = y0; out->w = 0; out->h = 0; }
        return false;
    }
    if (out) { out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0; }
    return true;
}

bool rect_contains_point(const rect_t *r, int x, int y) {
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

void rect_union(const rect_t *a, const rect_t *b, rect_t *out) {
    int x0 = a->x < b->x ? a->x : b->x;
    int y0 = a->y < b->y ? a->y : b->y;
    int ax1 = a->x + a->w, bx1 = b->x + b->w;
    int ay1 = a->y + a->h, by1 = b->y + b->h;
    int x1 = ax1 > bx1 ? ax1 : bx1;
    int y1 = ay1 > by1 ? ay1 : by1;
    if (out) { out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0; }
}

bool rect_overlap(const rect_t *a, const rect_t *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

void rect_inset(rect_t *r, int dx, int dy) {
    r->x += dx;
    r->y += dy;
    r->w -= dx * 2;
    r->h -= dy * 2;
}
