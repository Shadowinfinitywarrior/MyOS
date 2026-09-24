#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "../include/types.h"

typedef struct ring_buffer {
    uint8_t *data;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} ring_buffer_t;

void    ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint32_t size);
int     ring_buffer_put(ring_buffer_t *rb, uint8_t byte);
int     ring_buffer_get(ring_buffer_t *rb, uint8_t *byte);
int     ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint32_t len);
int     ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint32_t len);
uint32_t ring_buffer_available(ring_buffer_t *rb);
uint32_t ring_buffer_used(ring_buffer_t *rb);
void    ring_buffer_clear(ring_buffer_t *rb);

#endif

