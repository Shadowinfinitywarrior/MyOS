#ifndef GUI_RECT_H
#define GUI_RECT_H

#include "../include/types.h"

typedef struct rect {
    int x, y, w, h;
} rect_t;

static inline rect_t rect_make(int x, int y, int w, int h) {
    rect_t r = { x, y, w, h };
    return r;
}

static inline bool rect_is_empty(const rect_t *r) {
    return !r || r->w <= 0 || r->h <= 0;
}

bool rect_intersect(const rect_t *a, const rect_t *b, rect_t *out);
bool rect_contains_point(const rect_t *r, int x, int y);
void rect_union(const rect_t *a, const rect_t *b, rect_t *out);
bool rect_overlap(const rect_t *a, const rect_t *b);
void rect_inset(rect_t *r, int dx, int dy);

/* Translate a rect by (dx,dy) in place. */
static inline void rect_offset(rect_t *r, int dx, int dy) {
    r->x += dx;
    r->y += dy;
}

#endif
