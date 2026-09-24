#include "ring_buffer.h"
#include "string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint32_t size) {
    rb->data = buf;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

int ring_buffer_put(ring_buffer_t *rb, uint8_t byte) {
    if (rb->count >= rb->size) return -1;   /* Full */
    rb->data[rb->head] = byte;
    rb->head = (rb->head + 1) % rb->size;
    rb->count++;
    return 0;
}

int ring_buffer_get(ring_buffer_t *rb, uint8_t *byte) {
    if (rb->count == 0) return -1;          /* Empty */
    *byte = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    rb->count--;
    return 0;
}

int ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint32_t len) {
    uint32_t written = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (ring_buffer_put(rb, data[i]) != 0) break;
        written++;
    }
    return written;
}

int ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint32_t len) {
    uint32_t read = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (ring_buffer_get(rb, &data[i]) != 0) break;
        read++;
    }
    return read;
}

uint32_t ring_buffer_available(ring_buffer_t *rb) {
    return rb->size - rb->count;
}

uint32_t ring_buffer_used(ring_buffer_t *rb) {
    return rb->count;
}

void ring_buffer_clear(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

