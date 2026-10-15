#ifndef ASLR_H
#define ASLR_H

#include "../include/types.h"

void aslr_init(void);
uint64_t aslr_rand(void);
uint64_t aslr_exec_base(void);
uint64_t aslr_stack_base(void);
uint64_t aslr_mmap_base(void);

#endif
