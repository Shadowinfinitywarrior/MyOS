#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include "types.h"
#include "../include/system.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <string.h>

#define HEAP_MAGIC 0x48454150
#define BLOCK_FREE 0x0
#define BLOCK_USED 0x1
#define PAGE_SIZE 4096

typedef struct block {
    uint32_t magic;
    uint32_t total_pages;
    uint32_t state;
    uint32_t _pad;
    struct block *prev;
    struct block *next;
} PACKED block_t;

/* The kernel's boot page tables and the PMM start handing out pages right at
 * __kernel_end. The heap previously started there too, so its block headers
 * clobbered the PML4/PDPT (the crash showed "HEAP" magic in the PML4). Start
 * the heap at a fixed high identity-mapped address far above those pages. */
#define HEAP_BASE 0x4000000UL  /* 64 MB */
#define HEAP_SIZE  (16UL * 1024 * 1024)  /* 16 MiB heap */
#define HEAP_END   (HEAP_BASE + HEAP_SIZE)

static uintptr_t next_free_addr = 0;
static block_t *free_head = NULL;
static volatile int heap_lock = 0;

static inline uint8_t heap_enter(void) {
    const uint64_t eflags = read_eflags();
    cli();
    while (__atomic_test_and_set((int *)&heap_lock, __ATOMIC_ACQUIRE)) {
    }
    return (eflags >> 9) & 1;
}

static inline void heap_exit(uint8_t if_bit) {
    __atomic_clear((int *)&heap_lock, __ATOMIC_RELEASE);
    if (if_bit) sti();
}

static void heap_ensure_init(void) {
    if (next_free_addr == 0) {
        next_free_addr = HEAP_BASE;
    }
}

void heap_init(uint32_t start, uint32_t size) {
    (void)start; (void)size;
    /* The heap resides at VA==PA 0x4000000..0x5000000 inside the allocatable
     * E820 region. Claim those frames in the PMM now so the allocator can never
     * hand them out for page tables/DMA and then have them clobbered by heap
     * writes through the identity mapping (or vice versa). */
    int reserved = pmm_reserve_range(HEAP_BASE, HEAP_END);
    if (reserved <= 0) {
        kprintf("[HEAP] WARNING: failed to reserve heap frames in PMM (reserved=%d)\n", reserved);
    }
    next_free_addr = HEAP_BASE;
    kprintf("[HEAP] init base=0x%lx size=%lu (fixed)\n", (unsigned long)HEAP_BASE, (unsigned long)HEAP_SIZE);
    heap_ensure_init();
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    heap_ensure_init();
    if (size > (size_t)(-1) - PAGE_SIZE) return NULL;
    size_t data_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t need = data_pages + 1;
    if (need < 2) need = 2;
    if (need > 0xFFFF) return NULL;

    uint8_t if_bit = heap_enter();
    block_t *blk = free_head;
    while (blk) {
        if (blk->magic != HEAP_MAGIC) { heap_exit(if_bit); return NULL; }
        if (blk->total_pages >= need) break;
        blk = blk->next;
    }

    if (blk) {
        size_t leftover = blk->total_pages - need;
        if (leftover >= 2) {
            block_t *rest = (block_t *)((uintptr_t)blk + need * PAGE_SIZE);
            rest->magic = HEAP_MAGIC;
            rest->total_pages = (uint32_t)leftover;
            rest->state = BLOCK_FREE;
            rest->prev = blk->prev;
            rest->next = blk->next;
            if (rest->prev) rest->prev->next = rest;
            else free_head = rest;
            if (rest->next) rest->next->prev = rest;
            blk->total_pages = (uint32_t)need;
        } else {
            if (blk->prev) blk->prev->next = blk->next;
            else free_head = blk->next;
            if (blk->next) blk->next->prev = blk->prev;
        }
        blk->state = BLOCK_USED;
        blk->prev = blk->next = NULL;
        heap_exit(if_bit);
        memset((void *)((uintptr_t)blk + PAGE_SIZE), 0, data_pages * PAGE_SIZE);
        return (void *)((uintptr_t)blk + PAGE_SIZE);
    }

    blk = (block_t *)next_free_addr;
    if ((uintptr_t)next_free_addr + need * PAGE_SIZE > HEAP_END) {
        heap_exit(if_bit);
        return NULL;
    }
    next_free_addr += need * PAGE_SIZE;
    heap_exit(if_bit);
    memset(blk, 0, need * PAGE_SIZE);
    blk->magic = HEAP_MAGIC;
    blk->total_pages = (uint32_t)need;
    blk->state = BLOCK_USED;
    blk->prev = blk->next = NULL;
    return (void *)((uintptr_t)blk + PAGE_SIZE);
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *blk = (block_t *)((uintptr_t)ptr - PAGE_SIZE);
    if ((uintptr_t)blk < HEAP_BASE || (uintptr_t)blk >= HEAP_END) {
        return;
    }
    uint8_t if_bit = heap_enter();
    if (blk->magic != HEAP_MAGIC) { heap_exit(if_bit); return; }
    if (blk->state == BLOCK_FREE) { heap_exit(if_bit); return; }
    blk->state = BLOCK_FREE;
    block_t *after = free_head, *before = NULL;
    while (after && (uintptr_t)after < (uintptr_t)blk) {
        before = after;
        after = after->next;
    }
    blk->prev = before;
    blk->next = after;
    if (before) before->next = blk;
    else free_head = blk;
    if (after) after->prev = blk;
    if (after && (uintptr_t)blk + blk->total_pages * PAGE_SIZE == (uintptr_t)after) {
        blk->total_pages += after->total_pages;
        blk->next = after->next;
        if (after->next) after->next->prev = blk;
    }
    if (before && (uintptr_t)before + before->total_pages * PAGE_SIZE == (uintptr_t)blk) {
        before->total_pages += blk->total_pages;
        before->next = blk->next;
        if (blk->next) blk->next->prev = before;
    }
    heap_exit(if_bit);
}
