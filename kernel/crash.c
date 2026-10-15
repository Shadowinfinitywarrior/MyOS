#include "crash.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/rtc.h"

static crash_dump_t g_crash_dump __attribute__((section(".data")));

static inline bool is_valid_stack_ptr(uintptr_t ptr) {
    if ((ptr & 0x7) != 0) return false;
    /* Must not be in the null-page / low BIOS memory (< 64KB) */
    if (ptr < 0x10000) return false;
    /* x86-64 canonical check:
     * Lower canonical half: 0x0000000000000000 .. 0x00007FFFFFFFFFFF
     * Upper canonical half: 0xFFFF800000000000 .. 0xFFFFFFFFFFFFFFFF
     * Any address in the middle is non-canonical and causes #GP (exception 13) if dereferenced! */
    if (ptr > 0x00007FFFFFFFFFFFULL && ptr < 0xFFFF800000000000ULL) return false;
    return true;
}

static uint32_t walk_stack(uint64_t rbp, uint64_t *out_frames, uint32_t max_depth) {
    uint32_t depth = 0;
    uintptr_t cur_frame = (uintptr_t)rbp;

    while (cur_frame && depth < max_depth) {
        if (!is_valid_stack_ptr(cur_frame)) break;

        uint64_t *frame = (uint64_t *)cur_frame;
        uint64_t next_frame = frame[0];
        uint64_t rip = frame[1];

        /* Only record canonical instruction pointers */
        if (is_valid_stack_ptr(rip)) {
            out_frames[depth++] = rip;
        } else {
            break;
        }

        /* Stack frames MUST advance upwards in memory (stacks grow downwards).
         * Disallow loops, backward pointers, or huge jumps (> 64 KB). */
        if (next_frame <= cur_frame || next_frame > cur_frame + 0x10000) {
            break;
        }
        cur_frame = (uintptr_t)next_frame;
    }
    return depth;
}

void crash_dump_save(const char *msg, registers_t *regs) {
    static volatile int in_crash_save = 0;
    if (in_crash_save) return;
    in_crash_save = 1;

    g_crash_dump.magic = CRASH_DUMP_MAGIC;
    g_crash_dump.version = 1;
    g_crash_dump.timestamp = 0;

    if (regs) {
        g_crash_dump.regs = *regs;
        if (is_valid_stack_ptr(regs->rbp)) {
            g_crash_dump.stack_depth = walk_stack(regs->rbp, g_crash_dump.stack_trace, 64);
        } else {
            g_crash_dump.stack_depth = 0;
        }
    } else {
        memset(&g_crash_dump.regs, 0, sizeof(registers_t));
        uint64_t cur_rbp = 0;
        __asm__ volatile("movq %%rbp, %0" : "=r"(cur_rbp));
        if (is_valid_stack_ptr(cur_rbp)) {
            g_crash_dump.stack_depth = walk_stack(cur_rbp, g_crash_dump.stack_trace, 64);
        } else {
            g_crash_dump.stack_depth = 0;
        }
    }

    if (msg) {
        strncpy(g_crash_dump.panic_msg, msg, sizeof(g_crash_dump.panic_msg) - 1);
        g_crash_dump.panic_msg[sizeof(g_crash_dump.panic_msg) - 1] = '\0';
    }

    kprintf("[CRASH-DUMP] Recorded kernel crash dump: %s\n", g_crash_dump.panic_msg);
    if (regs) {
        kprintf("[CRASH-DUMP] RIP=0x%lx RSP=0x%lx RBP=0x%lx\n",
                (unsigned long)regs->rip, (unsigned long)regs->rsp, (unsigned long)regs->rbp);
    }
}

void crash_dump_check(void) {
    if (g_crash_dump.magic == CRASH_DUMP_MAGIC) {
        kprintf("[CRASH-RECOVERY] Previous boot panic detected: %s\n", g_crash_dump.panic_msg);
        kprintf("[CRASH-RECOVERY] RIP=0x%lx RSP=0x%lx\n",
                (unsigned long)g_crash_dump.regs.rip, (unsigned long)g_crash_dump.regs.rsp);
        g_crash_dump.magic = 0;
    }
}
