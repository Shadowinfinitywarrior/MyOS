#include "../include/system.h"
#include "../lib/printf.h"

__attribute__((section(".data")))
uint64_t __stack_chk_guard = 0;

static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void stack_protector_init(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    
    bool has_rdrand = (ecx & (1 << 30)) != 0;
    if (has_rdrand) {
        uint64_t rand_val;
        uint8_t ok;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(rand_val), "=qm"(ok));
        if (ok) {
            __stack_chk_guard = rand_val;
        } else {
            __stack_chk_guard = read_tsc() ^ 0xDEADBEEFCAFEBABEULL;
        }
    } else {
        __stack_chk_guard = read_tsc() ^ 0xDEADBEEFCAFEBABEULL;
    }

    /* Ensure low byte is 0 to stop C-string overflows */
    ((uint8_t *)&__stack_chk_guard)[0] = 0;
}

__attribute__((noreturn))
void __stack_chk_fail(void) {
    PANIC("STACK SMASH DETECTED");
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

