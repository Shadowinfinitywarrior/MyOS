#ifndef HEAP_H
#define HEAP_H

#include "../include/types.h"

void heap_init(uint32_t start, uint32_t size);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);

#endif
