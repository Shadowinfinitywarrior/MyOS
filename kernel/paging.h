#ifndef PAGING_H
#define PAGING_H

#include "../include/types.h"

#define PAGE_SIZE 4096
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_NOCACHE 16
#define PAGE_NX (1ULL << 63)

/* A page-directory handle. This is NOT the page table itself: the tables are
 * reached through physical addresses, so all this needs to carry is the
 * physical PML4 frame plus an intrusive link used by the slot pool's
 * free/live lists. Every user of the type outside paging.c only ever holds a
 * pointer, so nothing depends on the old 8 KiB entries[] shadow. */
typedef struct page_directory {
    uint64_t pml4_phys;
    struct page_directory *next;
} page_directory_t;

void paging_init(void);
void paging_map(uint64_t virt, uint64_t phys, uint64_t flags);
void paging_unmap(uint64_t virt);
uint64_t paging_get_physical(uint64_t virt);
uint64_t paging_get_attrs(uint64_t virt);
page_directory_t *paging_get_directory(void);
page_directory_t *paging_get_active(void);
page_directory_t *paging_clone_directory(page_directory_t *src);
void paging_free_directory(page_directory_t *dir);
void paging_switch_directory(page_directory_t *dir);
void paging_dump_dirs(void);

#endif
