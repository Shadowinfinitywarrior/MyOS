#include "mylang.h"
#include "../kernel/heap.h"

void* mylang_alloc(size_t sz){
    return kmalloc(sz);
}

void mylang_free(void *p){
    kfree(p);
}
