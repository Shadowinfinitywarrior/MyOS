#include "bitmap.h"
#include "string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

void bitmap_init(bitmap_t *bm, uint32_t *data, uint32_t size) {
    bm->data = data;
    bm->size = size;
    bm->used = 0;
    memset(data, 0, (size + 31) / 32 * 4);
}

void bitmap_set(bitmap_t *bm, uint32_t bit) {
    if (bit >= bm->size) return;
    if (!(bm->data[bit / 32] & (1 << (bit % 32))))
        bm->used++;
    bm->data[bit / 32] |= (1 << (bit % 32));
}

void bitmap_clear(bitmap_t *bm, uint32_t bit) {
    if (bit >= bm->size) return;
    if (bm->data[bit / 32] & (1 << (bit % 32)))
        bm->used--;
    bm->data[bit / 32] &= ~(1 << (bit % 32));
}

int bitmap_test(bitmap_t *bm, uint32_t bit) {
    if (bit >= bm->size) return 0;
    return (bm->data[bit / 32] >> (bit % 32)) & 1;
}

uint32_t bitmap_find_first_free(bitmap_t *bm) {
    for (uint32_t i = 0; i < (bm->size + 31) / 32; i++) {
        if (bm->data[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                uint32_t bit = i * 32 + j;
                if (bit >= bm->size) return 0xFFFFFFFF;
                if (!(bm->data[i] & (1 << j)))
                    return bit;
            }
        }
    }
    return 0xFFFFFFFF;
}

uint32_t bitmap_find_contiguous(bitmap_t *bm, uint32_t count) {
    uint32_t run = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < bm->size; i++) {
        if (!bitmap_test(bm, i)) {
            if (run == 0) start = i;
            run++;
            if (run == count) return start;
        } else {
            run = 0;
        }
    }
    return 0xFFFFFFFF;
}

void bitmap_set_range(bitmap_t *bm, uint32_t start, uint32_t count) {
    for (uint32_t i = start; i < start + count && i < bm->size; i++)
        bitmap_set(bm, i);
}

void bitmap_clear_range(bitmap_t *bm, uint32_t start, uint32_t count) {
    for (uint32_t i = start; i < start + count && i < bm->size; i++)
        bitmap_clear(bm, i);
}

