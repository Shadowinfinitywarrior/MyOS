#include "mmap.h"
#include "paging.h"
#include "pmm.h"
#include "heap.h"
#include "process.h"
#include "shm.h"
#include "pagefault.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../fs/vfs.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MMAP_START  0x40000000
#define MMAP_END    0x80000000

static mmap_region_t *region_list = NULL;

void mmap_init(void) {
    region_list = NULL;
    kprintf("[MMAP] Memory mapping subsystem initialized\n");
}

static uint32_t find_free_region(uint32_t length) {
    uint32_t addr = MMAP_START;
    mmap_region_t *r = region_list;
    while (r) {
        if (addr + length <= r->start) return addr;
        addr = ALIGN_UP(r->start + r->length, PAGE_SIZE);
        r = r->next;
    }
    if (addr + length <= MMAP_END) return addr;
    return 0;
}

void *sys_mmap(process_t *proc, uint32_t addr, uint32_t length,
               uint32_t prot, uint32_t flags, int fd, uint32_t offset) {
    if (length == 0) return MAP_FAILED;
    length = ALIGN_UP(length, PAGE_SIZE);
    if (addr == 0 || !(flags & MAP_FIXED)) {
        addr = find_free_region(length);
        if (!addr) return MAP_FAILED;
    }

    /* Determine base page flags */
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) page_flags |= PAGE_WRITE;
    if (!(prot & PROT_EXEC)) page_flags |= PAGE_NX;

    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);

    /* Check if this is a shared memory fd */
    int shmid = get_shmid_from_fd(proc, fd);
    if (shmid >= 0) {
        /* Map shared memory segment */
        extern void *shm_attach(int shmid);
        void *shm_addr = shm_attach(shmid);
        if (!shm_addr) {
            paging_switch_directory(old);
            return MAP_FAILED;
        }
        /* Map the shared memory at the requested address */
        uint32_t pages = length / PAGE_SIZE;
        shm_segment_t *seg = shm_get_segments();
        if (!seg) {
            paging_switch_directory(old);
            return MAP_FAILED;
        }
        seg = &seg[shmid];
        for (uint32_t p = 0; p < pages; p++) {
            paging_map(addr + p * PAGE_SIZE, seg->phys_addr + p * PAGE_SIZE, page_flags);
        }
    } else if (flags & MAP_ANONYMOUS) {
        if (flags & MAP_PRIVATE) {
            /* MAP_PRIVATE with anonymous: use lazy allocation + COW.
             * Initially map as read-only (if writable) to trigger COW on first write.
             * Pages are allocated on demand via page fault. */
            uint64_t cow_flags = PAGE_PRESENT | PAGE_USER;
            if (!(prot & PROT_EXEC)) cow_flags |= PAGE_NX;
            /* For MAP_PRIVATE, start read-only to enable COW */
            if (prot & PROT_WRITE) {
                /* Don't set PAGE_WRITE - will trigger COW on first write */
            }
            /* Don't allocate physical pages yet - demand paging */
            for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
                /* Just set up the PTE as not-present to trigger demand page fault */
                /* Actually, we map a zero page read-only for MAP_PRIVATE to avoid
                 * allocating zero pages for every mapping. But simpler: allocate on fault. */
                /* For now, map as read-only pointing to a shared zero page? 
                 * Let's just map as not-present by not mapping at all.
                 * The page fault handler will allocate on demand. */
            }
        } else {
            /* MAP_SHARED anonymous: allocate immediately */
            for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) {
                    /* Cleanup on failure */
                    for (uint32_t b = addr; b < a; b += PAGE_SIZE) {
                        uint32_t p = paging_get_physical(b);
                        if (p) { pmm_free_page(p); paging_unmap(b); }
                    }
                    paging_switch_directory(old);
                    return MAP_FAILED;
                }
                paging_map(a, phys, page_flags);
                memset((void *)a, 0, PAGE_SIZE);
            }
        }
    } else if (fd >= 0) {
        /* File-backed mapping */
        if (flags & MAP_PRIVATE) {
            /* MAP_PRIVATE file-backed: use COW - map read-only initially */
            uint64_t cow_flags = PAGE_PRESENT | PAGE_USER;
            if (!(prot & PROT_EXEC)) cow_flags |= PAGE_NX;
            /* For simplicity, allocate and copy on demand via page fault */
            for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) {
                    for (uint32_t b = addr; b < a; b += PAGE_SIZE) {
                        uint32_t p = paging_get_physical(b);
                        if (p) { pmm_free_page(p); paging_unmap(b); }
                    }
                    paging_switch_directory(old);
                    return MAP_FAILED;
                }
                /* Zero for now - real implementation would read from file */
                memset((void *)a, 0, PAGE_SIZE);
                paging_map(a, phys, cow_flags);  /* Read-only for COW */
            }
        } else {
            /* MAP_SHARED file-backed: allocate immediately */
            for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) {
                    for (uint32_t b = addr; b < a; b += PAGE_SIZE) {
                        uint32_t p = paging_get_physical(b);
                        if (p) { pmm_free_page(p); paging_unmap(b); }
                    }
                    paging_switch_directory(old);
                    return MAP_FAILED;
                }
                memset((void *)a, 0, PAGE_SIZE);
                paging_map(a, phys, page_flags);
            }
        }
    }
    paging_switch_directory(old);

    /* Register VMA for page fault handling (demand paging + COW) */
    vma_add(addr, addr + length, prot, flags, fd, offset);

    mmap_region_t *region = (mmap_region_t *)kmalloc(sizeof(mmap_region_t));
    region->start = addr;
    region->length = length;
    region->prot = prot;
    region->flags = flags;
    region->fd = fd;
    region->offset = offset;
    region->next = NULL;

    mmap_region_t **pp = &region_list;
    while (*pp && (*pp)->start < addr) pp = &(*pp)->next;
    region->next = *pp;
    *pp = region;
    return (void *)addr;
}

int sys_munmap(process_t *proc, uint32_t addr, uint32_t length) {
    length = ALIGN_UP(length, PAGE_SIZE);
    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);
    for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
        uint32_t phys = paging_get_physical(a);
        if (phys) { pmm_free_page(phys); paging_unmap(a); }
    }
    paging_switch_directory(old);

    /* Remove VMA tracking */
    vma_remove(addr, addr + length);

    mmap_region_t **pp = &region_list;
    while (*pp) {
        if ((*pp)->start == addr && (*pp)->length == length) {
            mmap_region_t *r = *pp;
            *pp = r->next;
            kfree(r);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

int sys_mprotect(process_t *proc, uint32_t addr, uint32_t length, uint32_t prot) {
    length = ALIGN_UP(length, PAGE_SIZE);
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) page_flags |= PAGE_WRITE;
    if (!(prot & PROT_EXEC)) page_flags |= PAGE_NX;
    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);
    for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
        uint32_t phys = paging_get_physical(a);
        if (phys) paging_map(a, phys, page_flags);
    }
    paging_switch_directory(old);

    /* Update VMA protection */
    vma_update_prot(addr, addr + length, prot);
    return 0;
}