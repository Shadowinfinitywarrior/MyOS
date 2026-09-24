#include "mmap.h"
#include "paging.h"
#include "pmm.h"
#include "heap.h"
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
    uint32_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) page_flags |= PAGE_WRITE;

    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);

    if (flags & MAP_ANONYMOUS) {
        for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
            uint32_t phys = pmm_alloc_page();
            paging_map(a, phys, page_flags);
            memset((void *)a, 0, PAGE_SIZE);
        }
    } else if (fd >= 0) {
        /* Simplified file-backed */
        for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
            uint32_t phys = pmm_alloc_page();
            paging_map(a, phys, page_flags);
            memset((void *)a, 0, PAGE_SIZE);
        }
    }
    paging_switch_directory(old);

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
    uint32_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) page_flags |= PAGE_WRITE;
    page_directory_t *old = paging_get_active();
    if (proc && proc->page_dir) paging_switch_directory(proc->page_dir);
    for (uint32_t a = addr; a < addr + length; a += PAGE_SIZE) {
        uint32_t phys = paging_get_physical(a);
        if (phys) paging_map(a, phys, page_flags);
    }
    paging_switch_directory(old);
    return 0;
}
