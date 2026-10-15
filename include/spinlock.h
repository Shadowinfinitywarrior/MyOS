#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "types.h"
#include "system.h"

typedef struct {
    volatile uint32_t lock;
    uint32_t cpu;          /* owner CPU ID (debug) */
    uint32_t depth;        /* recursion depth (debug) */
} spinlock_t;

#define SPINLOCK_INIT { .lock = 0, .cpu = 0xFFFFFFFF, .depth = 0 }

static inline void spinlock_init(spinlock_t *s) {
    s->lock = 0;
    s->cpu = 0xFFFFFFFF;
    s->depth = 0;
}

static inline void spin_lock(spinlock_t *s) {
    while (1) {
        uint32_t old;
        __asm__ volatile(
            "lock xchgl %0, %1"
            : "=r"(old), "+m"(s->lock)
            : "0"(1)
            : "memory"
        );
        if (old == 0) break;

        while (s->lock) {
            __asm__ volatile("pause");
        }
    }
}

static inline void spin_unlock(spinlock_t *s) {
    __asm__ volatile("" ::: "memory");
    s->lock = 0;
}

static inline uint64_t spin_lock_irqsave(spinlock_t *s) {
    uint64_t flags;
    __asm__ volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    spin_lock(s);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *s, uint64_t flags) {
    spin_unlock(s);
    if (flags & (1ULL << 9)) {
        __asm__ volatile("sti");
    }
}

#endif
