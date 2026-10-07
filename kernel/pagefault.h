#ifndef PAGEFAULT_H
#define PAGEFAULT_H

#include "../include/types.h"
#include "process.h"

void pagefault_init(void);
void pagefault_handler(registers_t *regs);

/* VMA management for demand paging and COW */
void vma_add(uint64_t start, uint64_t end, uint32_t prot, uint32_t flags, int fd, uint64_t offset);
void vma_remove(uint64_t start, uint64_t end);
/* Update VMA protection (for mprotect) */
void vma_update_prot(uint64_t start, uint64_t end, uint32_t prot);

#endif