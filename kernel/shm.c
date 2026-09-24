#include "shm.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

typedef struct shm_segment {
    char name[SHM_NAME_MAX];
    uint32_t size;
    uint32_t phys_addr;
    uint32_t ref_count;
    bool in_use;
} shm_segment_t;

static shm_segment_t segments[SHM_MAX_SEGMENTS];

void shm_init(void) {
    memset(segments, 0, sizeof(segments));
    kprintf("[SHM] Shared memory initialized\n");
}

int shm_create(const char *name, uint32_t size) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].in_use && strcmp(segments[i].name, name) == 0) return -1;
    }
    int slot = -1;
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) if (!segments[i].in_use) { slot = i; break; }
    if (slot < 0) return -1;
    size = ALIGN_UP(size, PAGE_SIZE);
    uint32_t phys = 0;
    uint32_t pages = size / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        uint32_t page = pmm_alloc_page();
        if (p == 0) phys = page;
    }
    if (!phys) return -1;
    segments[slot].in_use = true;
    strncpy(segments[slot].name, name, SHM_NAME_MAX-1);
    segments[slot].size = size;
    segments[slot].phys_addr = phys;
    segments[slot].ref_count = 0;
    kprintf("[SHM] Created '%s': %u bytes\n", name, size);
    return slot;
}

int shm_open(const char *name) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].in_use && strcmp(segments[i].name, name) == 0) return i;
    }
    return -1;
}

void *shm_attach(int shmid) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return NULL;
    if (!segments[shmid].in_use) return NULL;
    shm_segment_t *seg = &segments[shmid];
    uint32_t virt = 0xC0000000 + shmid * 0x100000;
    uint32_t pages = seg->size / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        paging_map(virt + p*PAGE_SIZE, seg->phys_addr + p*PAGE_SIZE, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    seg->ref_count++;
    return (void *)virt;
}

int shm_detach(int shmid, void *addr) {
    (void)addr;
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return -1;
    segments[shmid].ref_count--;
    return 0;
}

int shm_destroy(int shmid) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return -1;
    if (!segments[shmid].in_use) return -1;
    if (segments[shmid].ref_count > 0) return -1;
    uint32_t pages = segments[shmid].size / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) pmm_free_page(segments[shmid].phys_addr + p*PAGE_SIZE);
    segments[shmid].in_use = false;
    return 0;
}
