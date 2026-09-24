#ifndef BITMAP_H
#define BITMAP_H

#include "../include/types.h"

typedef struct bitmap {
    uint32_t *data;
    uint32_t  size;     /* Total bits */
    uint32_t  used;     /* Set bits count */
} bitmap_t;

void     bitmap_init(bitmap_t *bm, uint32_t *data, uint32_t size);
void     bitmap_set(bitmap_t *bm, uint32_t bit);
void     bitmap_clear(bitmap_t *bm, uint32_t bit);
int      bitmap_test(bitmap_t *bm, uint32_t bit);
uint32_t bitmap_find_first_free(bitmap_t *bm);
uint32_t bitmap_find_contiguous(bitmap_t *bm, uint32_t count);
void     bitmap_set_range(bitmap_t *bm, uint32_t start, uint32_t count);
void     bitmap_clear_range(bitmap_t *bm, uint32_t start, uint32_t count);

#endif

