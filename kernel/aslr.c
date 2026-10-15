#include "aslr.h"
#include "../include/system.h"
#include "../lib/printf.h"

static uint64_t aslr_seed = 0x9e3779b97f4a7c15ULL;

static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void aslr_init(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    
    if (ecx & (1 << 30)) {
        uint64_t rand_val;
        uint8_t ok;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(rand_val), "=qm"(ok));
        if (ok) {
            aslr_seed ^= rand_val;
        } else {
            aslr_seed ^= read_tsc();
        }
    } else {
        aslr_seed ^= read_tsc() ^ (read_tsc() << 32);
    }

    if (aslr_seed == 0) aslr_seed = 0x123456789abcdef0ULL;
    kprintf("[ASLR] Address space layout randomization initialized (entropy seeded)\n");
}

uint64_t aslr_rand(void) {
    aslr_seed ^= aslr_seed << 13;
    aslr_seed ^= aslr_seed >> 7;
    aslr_seed ^= aslr_seed << 17;
    return aslr_seed;
}

uint64_t aslr_exec_base(void) {
    /* Randomize within default region, 4KB page aligned */
    uint64_t offset = (aslr_rand() % 4096) * 4096;
    return 0x40000000ULL + offset;
}

uint64_t aslr_stack_base(void) {
    /* Randomize user stack down from 0xBFFFF000 by up to 64MB, page aligned */
    uint64_t offset = (aslr_rand() % 16384) * 4096;
    return 0xBFFFF000ULL - offset;
}

uint64_t aslr_mmap_base(void) {
    /* Randomize high mmap space around 0x7F0000000000, page aligned */
    uint64_t offset = (aslr_rand() % 65536) * 4096;
    return 0x7F0000000000ULL + offset;
}
