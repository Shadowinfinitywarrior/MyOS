#include "shm.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static shm_segment_t segments[SHM_MAX_SEGMENTS];

/* Framebuffer shared memory segment index */
static int fb_shmid = -1;

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

/* shm_open - POSIX-style shared memory open */
int shm_open(const char *name) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].in_use && strcmp(segments[i].name, name) == 0) return i;
    }
    return -1;
}

/* shm_unlink - mark segment for destruction when last reference closes */
int shm_unlink(const char *name) {
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].in_use && strcmp(segments[i].name, name) == 0) {
            /* Mark as unlinked - will be destroyed when ref_count reaches 0 */
            segments[i].in_use = false;  /* Simplified: destroy immediately if no refs */
            if (segments[i].ref_count == 0) {
                uint32_t pages = segments[i].size / PAGE_SIZE;
                for (uint32_t p = 0; p < pages; p++) pmm_free_page(segments[i].phys_addr + p*PAGE_SIZE);
                memset(&segments[i], 0, sizeof(shm_segment_t));
            }
            return 0;
        }
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
    if (segments[shmid].ref_count > 0) segments[shmid].ref_count--;
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

/* Create framebuffer as shared memory segment */
int shm_create_framebuffer(uint32_t width, uint32_t height, uint32_t bpp) {
    uint32_t pitch = width * (bpp / 8);
    uint32_t size = pitch * height;
    size = ALIGN_UP(size, PAGE_SIZE);
    
    /* Check if framebuffer already exists */
    if (fb_shmid >= 0 && segments[fb_shmid].in_use) {
        return fb_shmid;
    }
    
    int slot = -1;
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) if (!segments[i].in_use) { slot = i; break; }
    if (slot < 0) return -1;
    
    uint32_t phys = 0;
    uint32_t pages = size / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        uint32_t page = pmm_alloc_page();
        if (p == 0) phys = page;
    }
    if (!phys) return -1;
    
    segments[slot].in_use = true;
    snprintf(segments[slot].name, SHM_NAME_MAX, "fb:%dx%d", width, height);
    segments[slot].size = size;
    segments[slot].phys_addr = phys;
    segments[slot].ref_count = 0;
    
    fb_shmid = slot;
    kprintf("[SHM] Framebuffer created: %dx%d @ %u bytes (shmid=%d, phys=0x%X)\n", 
            width, height, size, slot, phys);
    return slot;
}

/* Get framebuffer shared memory segment */
int shm_get_framebuffer(void) {
    return fb_shmid;
}

/* Map framebuffer into process address space via mmap */
void *shm_map_framebuffer(process_t *proc, uint32_t addr) {
    if (fb_shmid < 0 || !segments[fb_shmid].in_use) return NULL;
    
    shm_segment_t *seg = &segments[fb_shmid];
    uint32_t pages = seg->size / PAGE_SIZE;
    uint64_t page_flags = PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    
    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);
    
    for (uint32_t p = 0; p < pages; p++) {
        paging_map(addr + p * PAGE_SIZE, seg->phys_addr + p * PAGE_SIZE, page_flags);
    }
    
    paging_switch_directory(old);
    seg->ref_count++;
    
    kprintf("[SHM] Framebuffer mapped into PID %d at 0x%X\n", proc ? proc->pid : 0, addr);
    return (void *)addr;
}

shm_segment_t *shm_get_segments(void) {
    return segments;
}

int get_shmid_from_fd(process_t *proc, int fd) {
    if (!proc || fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    if (!proc->fd_table[fd].in_use) return -1;
    
    uintptr_t node_ptr = (uintptr_t)proc->fd_table[fd].node;
    if (!(node_ptr & 0x80000000)) return -1;
    return node_ptr & 0x7FFFFFFF;
}