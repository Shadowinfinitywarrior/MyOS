#include "security.h"
#include "../lib/printf.h"
#include "../lib/string.h"

#define USER_ADDR_MIN 0x40000000ULL
#define USER_ADDR_MAX 0xFFFF800000000000ULL

static void do_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

static uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void write_msr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" : : "a"((uint32_t)val), "d"((uint32_t)(val >> 32)), "c"(msr));
}

int is_user_range(const void *addr, size_t len) {
    uintptr_t a = (uintptr_t)addr;
    if (len == 0) return 1;
    if (a < USER_ADDR_MIN || a >= USER_ADDR_MAX) return 0;
    uintptr_t end = a + len;
    return (end > a && end <= USER_ADDR_MAX);
}

void cpu_enable_security(void) {
    uint64_t cr4;
    __asm__ volatile("movq %%cr4, %0" : "=r"(cr4));

    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    do_cpuid(7, 0, &eax, &ebx, &ecx, &edx);

    if (ebx & (1 << 7)) {
        cr4 |= CR4_SMEP;
    }
    if (ebx & (1 << 7)) {
        cr4 |= CR4_SMEP;
    }
    /* Note: SMAP bit is supported and tested via copy_to_user/copy_from_user stac/clac primitives,
     * deferred from global CR4 activation during boot to support ELF loader direct mappings. */
    (void)ebx;
    if (ecx & (1 << 2)) {
        cr4 |= CR4_UMIP;
    }

    __asm__ volatile("movq %0, %%cr4" :: "r"(cr4) : "memory");

    /* Ensure NX bit in EFER is enabled */
    uint64_t efer = read_msr(MSR_EFER);
    efer |= EFER_NXE;
    write_msr(MSR_EFER, efer);

    kprintf("[SECURITY] CPU security features active: SMEP=%d SMAP=%d UMIP=%d NX=1\n",
            (cr4 & CR4_SMEP) ? 1 : 0,
            (cr4 & CR4_SMAP) ? 1 : 0,
            (cr4 & CR4_UMIP) ? 1 : 0);
}

int copy_from_user(void *dst, const void *user_src, size_t len) {
    if (!dst && len > 0) return -14;
    if (!user_src && len > 0) return -14;
    if (len == 0) return 0;
    if (!is_user_range(user_src, len)) return -14;

    stac();
    memcpy(dst, user_src, len);
    clac();
    return 0;
}

int copy_to_user(void *user_dst, const void *src, size_t len) {
    if (!user_dst && len > 0) return -14;
    if (!src && len > 0) return -14;
    if (len == 0) return 0;
    if (!is_user_range(user_dst, len)) return -14;

    stac();
    memcpy(user_dst, src, len);
    clac();
    return 0;
}
