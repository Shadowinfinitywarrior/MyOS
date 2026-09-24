#ifndef PAGING_H
#define PAGING_H

#include "../include/types.h"

#define PAGE_SIZE 4096
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_NOCACHE 16

typedef struct page_directory {
    uint64_t entries[1024];
} page_directory_t;

void paging_init(void);
void paging_map(uint64_t virt, uint64_t phys, uint32_t flags);
void paging_unmap(uint64_t virt);
uint64_t paging_get_physical(uint64_t virt);
page_directory_t *paging_get_directory(void);
page_directory_t *paging_get_active(void);
page_directory_t *paging_clone_directory(page_directory_t *src);
void paging_switch_directory(page_directory_t *dir);
void paging_dump_dirs(void);

#endif
