#ifndef TYPES64_H
#define TYPES64_H

#include "types.h"

#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (2*1024*1024)
#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

#undef BIT
#define BIT(n) (1ULL << (n))

static inline uint64_t read_cr0(void){ uint64_t v; __asm__ volatile("mov %%cr0,%0":"=r"(v)); return v; }
static inline uint64_t read_cr3(void){ uint64_t v; __asm__ volatile("mov %%cr3,%0":"=r"(v)); return v; }
static inline uint64_t read_cr4(void){ uint64_t v; __asm__ volatile("mov %%cr4,%0":"=r"(v)); return v; }
static inline void write_cr3(uint64_t v){ __asm__ volatile("mov %0,%%cr3"::"r"(v)); }
static inline void invlpg(uint64_t addr){ __asm__ volatile("invlpg (%0)"::"r"(addr):"memory"); }

static inline uint64_t rdmsr(uint32_t msr){ uint32_t lo,hi; __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(msr)); return ((uint64_t)hi<<32)|lo; }
static inline void wrmsr(uint32_t msr, uint64_t v){ __asm__ volatile("wrmsr"::"a"((uint32_t)v),"d"((uint32_t)(v>>32)),"c"(msr)); }

#endif
