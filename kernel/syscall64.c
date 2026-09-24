#include "../lib/printf.h"
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MSR_EFER            0xC0000080
#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_FMASK           0xC0000084
#define MSR_GS_BASE         0xC0000101
#define MSR_KERNEL_GS_BASE  0xC0000102

#define EFER_SCE            (1ULL << 0)

/* SYSCALL entry point (context_switch.asm). */
extern void syscall_entry64(void);

/* Per-CPU SYSCALL data.  Kept in plain (absolute-address) memory rather than
 * a GS-relative cell: this kernel observed that WRMSR to IA32_GS_BASE does
 * not update the active %gs segment base here, so gs:0 addressing is not
 * reliable.  syscall_cpu[0] = kernel stack top; [1..4] = entry scratch. */
uint64_t syscall_cpu[8] __attribute__((aligned(16)));

void syscall_set_kernel_stack(uintptr_t stack_top) {
    syscall_cpu[0] = stack_top;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init_64(void) {
    /* Enable the SYSCALL/SYSRET instructions (EFER.SCE). */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);

    /* STAR[47:32] = kernel CS base (SYSCALL uses CS=0x08, SS=0x10).
     * STAR[63:48] = user CS base (SYSRET computes user CS = base+16, SS = base+8).
     * GDT: user code index 4 (0x20|RPL3 = 0x23), user data index 3 (0x18|RPL3 = 0x1B).
     * For SYSRET: CS = 0x13+16 = 0x23, SS = 0x13+8 = 0x1B. */
    uint64_t star = ((uint64_t)0x13 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    /* SYSCALL entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry64);

    /* Mask TF|IF|DF|NT|AC on SYSCALL entry (0x44700) */
    wrmsr(MSR_FMASK, 0x44700);

    /* SWAPGS pairs are not used by this kernel (see note above); the MSR values
     * are still programmed so a future per-CPU GS base works on hardware that
     * honors WRMSR for the active base. */
    wrmsr(MSR_GS_BASE, (uint64_t)syscall_cpu);
    wrmsr(MSR_KERNEL_GS_BASE, 0);

    kprintf("[SYSCALL] SYSCALL/SYSRET configured\n");
}