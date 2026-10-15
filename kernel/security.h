#ifndef SECURITY_H
#define SECURITY_H

#include "../include/types.h"
#include "../include/system.h"

#define CR4_UMIP (1UL << 11)
#define CR4_SMEP (1UL << 20)
#define CR4_SMAP (1UL << 21)

#define MSR_EFER 0xC0000080
#define EFER_NXE (1ULL << 11)

void cpu_enable_security(void);
int copy_from_user(void *dst, const void *user_src, size_t len);
int copy_to_user(void *user_dst, const void *src, size_t len);
int is_user_range(const void *addr, size_t len);

void stack_protector_init(void);

#endif
