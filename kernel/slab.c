#include "slab.h"
#include "pmm.h"
#include "heap.h"
#include "../lib/printf.h"
#include "../lib/string.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

typedef struct slab_obj {
    struct slab_obj *next_free;
} slab_obj_t;

typedef struct slab {
    struct slab *next;
    struct slab *prev;
    void *mem;
    slab_obj_t *free_list;
    uint32_t obj_size;
    uint32_t total_objs;
    uint32_t free_count;
} slab_t;

typedef struct {
    spinlock_t lock;
    uint32_t obj_size;
    slab_t *partial_slabs;
    slab_t *full_slabs;
    slab_t *free_slabs;
} kmem_cache_t;

static kmem_cache_t caches[SLAB_NUM_CACHES];

static void slab_list_add(slab_t **head, slab_t *s) {
    s->next = *head;
    s->prev = NULL;
    if (*head) (*head)->prev = s;
    *head = s;
}

static void slab_list_remove(slab_t **head, slab_t *s) {
    if (s->prev) s->prev->next = s->next;
    else *head = s->next;
    if (s->next) s->next->prev = s->prev;
    s->next = NULL;
    s->prev = NULL;
}

static slab_t *slab_create(uint32_t obj_size) {
    uint64_t page = pmm_alloc_page();
    if (!page) return NULL;
    memset((void*)(uintptr_t)page, 0, 4096);

    slab_t *s = (slab_t *)kmalloc(sizeof(slab_t));
    if (!s) {
        pmm_free_page(page);
        return NULL;
    }
    s->mem = (void*)(uintptr_t)page;
    s->obj_size = obj_size;
    s->total_objs = 4096 / obj_size;
    s->free_count = s->total_objs;
    s->next = NULL;
    s->prev = NULL;

    /* Initialize free list */
    uint8_t *ptr = (uint8_t *)s->mem;
    s->free_list = (slab_obj_t *)ptr;
    for (uint32_t i = 0; i < s->total_objs - 1; i++) {
        slab_obj_t *curr = (slab_obj_t *)(ptr + i * obj_size);
        curr->next_free = (slab_obj_t *)(ptr + (i + 1) * obj_size);
    }
    slab_obj_t *last = (slab_obj_t *)(ptr + (s->total_objs - 1) * obj_size);
    last->next_free = NULL;

    return s;
}

void slab_init(void) {
    for (int i = 0; i < SLAB_NUM_CACHES; i++) {
        caches[i].obj_size = 1U << (i + SLAB_MIN_ORDER);
        spinlock_init(&caches[i].lock);
        caches[i].partial_slabs = NULL;
        caches[i].full_slabs = NULL;
        caches[i].free_slabs = NULL;
    }
    kprintf("[SLAB] Initialized kmem caches for orders %d..%d (32B to 4KB)\n",
            SLAB_MIN_ORDER, SLAB_MAX_ORDER);
}

void *slab_alloc(uint32_t size) {
    if (size == 0) return NULL;
    uint32_t actual = SLAB_MIN_ORDER;
    while ((1U << actual) < size && actual <= SLAB_MAX_ORDER) actual++;
    if (actual > SLAB_MAX_ORDER) return kmalloc(size);

    int idx = actual - SLAB_MIN_ORDER;
    kmem_cache_t *cache = &caches[idx];
    uint64_t flags = spin_lock_irqsave(&cache->lock);

    slab_t *slab = cache->partial_slabs;
    if (!slab) {
        if (cache->free_slabs) {
            slab = cache->free_slabs;
            slab_list_remove(&cache->free_slabs, slab);
            slab_list_add(&cache->partial_slabs, slab);
        } else {
            slab = slab_create(cache->obj_size);
            if (!slab) {
                spin_unlock_irqrestore(&cache->lock, flags);
                return kmalloc(size);
            }
            slab_list_add(&cache->partial_slabs, slab);
        }
    }

    slab_obj_t *obj = slab->free_list;
    if (!obj) {
        spin_unlock_irqrestore(&cache->lock, flags);
        return kmalloc(size);
    }
    slab->free_list = obj->next_free;
    slab->free_count--;

    if (slab->free_count == 0) {
        slab_list_remove(&cache->partial_slabs, slab);
        slab_list_add(&cache->full_slabs, slab);
    }

    spin_unlock_irqrestore(&cache->lock, flags);
    return (void *)obj;
}

void slab_free(void *ptr, uint32_t size) {
    if (!ptr) return;
    if (size == 0 || size > (1U << SLAB_MAX_ORDER)) {
        kfree(ptr);
        return;
    }
    uint32_t actual = SLAB_MIN_ORDER;
    while ((1U << actual) < size && actual <= SLAB_MAX_ORDER) actual++;
    if (actual > SLAB_MAX_ORDER) {
        kfree(ptr);
        return;
    }

    int idx = actual - SLAB_MIN_ORDER;
    kmem_cache_t *cache = &caches[idx];
    uint64_t flags = spin_lock_irqsave(&cache->lock);

    /* Look in full slabs, then partial slabs */
    slab_t *target = NULL;
    uintptr_t p = (uintptr_t)ptr;
    for (slab_t *s = cache->full_slabs; s; s = s->next) {
        uintptr_t smem = (uintptr_t)s->mem;
        if (p >= smem && p < smem + 4096) {
            target = s;
            slab_list_remove(&cache->full_slabs, s);
            slab_list_add(&cache->partial_slabs, s);
            break;
        }
    }
    if (!target) {
        for (slab_t *s = cache->partial_slabs; s; s = s->next) {
            uintptr_t smem = (uintptr_t)s->mem;
            if (p >= smem && p < smem + 4096) {
                target = s;
                break;
            }
        }
    }

    if (target) {
        slab_obj_t *obj = (slab_obj_t *)ptr;
        obj->next_free = target->free_list;
        target->free_list = obj;
        target->free_count++;

        if (target->free_count == target->total_objs) {
            slab_list_remove(&cache->partial_slabs, target);
            slab_list_add(&cache->free_slabs, target);
        }
    } else {
        kfree(ptr);
    }

    spin_unlock_irqrestore(&cache->lock, flags);
}
