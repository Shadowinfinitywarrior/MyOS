#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

void pmm_init(uint64_t mmap_addr, uint64_t mmap_count);
int pmm_reserve_range(uint64_t start, uint64_t end);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t phys);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_total_pages(void);

#endif
