#include "pmm.h"
#include "../include/types.h"
#include "../include/system.h"
#include "../lib/printf.h"

#define PAGE_SIZE 4096
#define MAX_PAGES 262144  /* 1 GiB */
#define MAX_REGIONS 16

extern char __kernel_end[];

static uint8_t bitmap[MAX_PAGES / 8];
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;
static uint64_t region_start[MAX_REGIONS];
static uint64_t region_pages[MAX_REGIONS];
static uint32_t region_count = 0;

static inline void bitmap_set(uint64_t idx) { bitmap[idx >> 3] |= 1u << (idx & 7); }
static inline void bitmap_clear(uint64_t idx) { bitmap[idx >> 3] &= ~(1u << (idx & 7)); }
static inline int bitmap_test(uint64_t idx) { return bitmap[idx >> 3] & (1u << (idx & 7)); }

void pmm_init(uint64_t mmap_addr, uint64_t mmap_count) {
    uint64_t kernel_end = (uint64_t)(uintptr_t)__kernel_end;
    region_count = 0;
    total_pages = 0;
    if (mmap_addr && mmap_count > 0) {
        typedef struct {
            uint64_t base;
            uint64_t length;
            uint32_t type;
            uint32_t acpi;
        } __attribute__((packed)) e820_entry_t;
        e820_entry_t *map = (e820_entry_t *)(uintptr_t)mmap_addr;
        for (uint64_t i = 0; i < mmap_count && region_count < MAX_REGIONS; i++) {
            uint32_t type = map[i].type;
            if (type != 1) continue;
            uint64_t base = map[i].base;
            uint64_t length = map[i].length;
            if (length == 0) continue;
            uint64_t region_start_addr = base;
            uint64_t region_end = base + length;
            if (region_end <= kernel_end) continue;
            if (region_start_addr < kernel_end) {
                /* Round up to next page boundary after kernel_end */
                region_start_addr = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                if (region_start_addr <= kernel_end) region_start_addr += PAGE_SIZE;
            }
            if (region_start_addr >= region_end) continue;
            uint64_t usable_pages = (region_end - region_start_addr) / PAGE_SIZE;
            region_start[region_count] = region_start_addr;
            region_pages[region_count] = usable_pages;
            total_pages += usable_pages;
            region_count++;
        }
    }
    if (total_pages == 0) {
        /* Round up kernel_end to next page boundary */
        uint64_t region_start_addr = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (region_start_addr <= kernel_end) region_start_addr += PAGE_SIZE;
        uint64_t region_end = 256 * 1024 * 1024;
        if (region_start_addr < region_end) {
            uint64_t usable_pages = (region_end - region_start_addr) / PAGE_SIZE;
            if (usable_pages > MAX_PAGES) usable_pages = MAX_PAGES;
            region_start[0] = region_start_addr;
            region_pages[0] = usable_pages;
            total_pages = usable_pages;
            region_count = 1;
        }
    }
    if (total_pages == 0) {
        return;
    }
    if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;
    for (uint64_t i = 0; i < total_pages; i++) {
        bitmap_clear(i);
    }
    free_pages = total_pages;
    kprintf("[PMM] init: %u region(s), total_pages=%llu, free_pages=%llu\n", region_count, (unsigned long long)total_pages, (unsigned long long)free_pages);
    for (uint32_t r = 0; r < region_count; r++) {
        kprintf("[PMM] region %u: start=0x%llx pages=%llu end=0x%llx\n", r, (unsigned long long)region_start[r], (unsigned long long)region_pages[r], (unsigned long long)(region_start[r] + region_pages[r]*PAGE_SIZE));
    }
    if (region_count == 1 && region_start[0] >= ((kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) && region_start[0] < 256*1024*1024) {
        kprintf("[PMM] using fallback region after __kernel_end\n");
    }
}

int pmm_reserve_range(uint64_t start, uint64_t end) {
    if (start >= end) return -1;
    uint64_t r_start = start & ~((uint64_t)PAGE_SIZE - 1);
    uint64_t r_end = (end + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
    int reserved = 0;
    for (uint32_t r = 0; r < region_count; r++) {
        uint64_t rs = region_start[r];
        uint64_t re = rs + region_pages[r] * PAGE_SIZE;
        uint64_t lo = r_start > rs ? r_start : rs;
        uint64_t hi = r_end < re ? r_end : re;
        if (lo >= hi) continue;
        uint64_t offset = 0;
        for (uint32_t k = 0; k < r; k++) offset += region_pages[k];
        for (uint64_t page = lo; page < hi; page += PAGE_SIZE) {
            uint64_t idx = offset + (page - rs) / PAGE_SIZE;
            if (idx >= total_pages) continue;
            if (!bitmap_test(idx)) {
                bitmap_set(idx);
                free_pages--;
            }
            reserved++;
        }
    }
    return reserved;
}

uint64_t pmm_alloc_page(void) {
    /* Serialize against interrupt-context allocations (timer ISR, page-fault
     * path) on the BSP. A real SMP implementation must upgrade this to a
     * per-page-manager spinlock with IRQ-save/restore. */
    const uint64_t eflags = read_eflags();
    cli();
    uint64_t result = 0;
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            uint64_t offset = 0;
            for (uint32_t r = 0; r < region_count; r++) {
                uint64_t rp = region_pages[r];
                if (i < offset + rp) {
                    uint64_t page_in_region = i - offset;
                    result = region_start[r] + page_in_region * PAGE_SIZE;
                    break;
                }
                offset += rp;
            }
            break;
        }
    }
    if (eflags & (1ULL << 9)) sti();
    return result;
}

void pmm_free_page(uint64_t phys) {
    if (phys == 0) return;
    const uint64_t eflags = read_eflags();
    cli();
    for (uint32_t r = 0; r < region_count; r++) {
        uint64_t start = region_start[r];
        uint64_t end = start + region_pages[r] * PAGE_SIZE;
        if (phys >= start && phys < end && (phys % PAGE_SIZE) == 0) {
            uint64_t page_in_region = (phys - start) / PAGE_SIZE;
            uint64_t offset = 0;
            for (uint32_t k = 0; k < r; k++) offset += region_pages[k];
            uint64_t idx = offset + page_in_region;
            if (idx >= total_pages) break;
            if (!bitmap_test(idx)) break;
            bitmap_clear(idx);
            free_pages++;
            break;
        }
    }
    if (eflags & (1ULL << 9)) sti();
}

uint64_t pmm_get_free_pages(void) { return free_pages; }
uint64_t pmm_get_total_pages(void) { return total_pages; }
