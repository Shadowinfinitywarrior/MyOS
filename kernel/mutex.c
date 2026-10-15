#include "mutex.h"
#include "process.h"
#include "scheduler.h"

void mutex_init(mutex_t *m) {
    if (!m) return;
    spinlock_init(&m->guard);
    m->locked = 0;
    wait_queue_init(&m->waiters);
}

int mutex_trylock(mutex_t *m) {
    if (!m) return 0;
    uint64_t flags = spin_lock_irqsave(&m->guard);
    if (!m->locked) {
        m->locked = 1;
        spin_unlock_irqrestore(&m->guard, flags);
        return 1;
    }
    spin_unlock_irqrestore(&m->guard, flags);
    return 0;
}

void mutex_lock(mutex_t *m) {
    if (!m) return;
    uint64_t flags = spin_lock_irqsave(&m->guard);
    while (m->locked) {
        process_t *curr = process_get_current();
        if (curr && curr->pid != 0) {
            curr->state = PROC_BLOCKED;
            wait_queue_add(&m->waiters, curr);
            spin_unlock_irqrestore(&m->guard, flags);
            scheduler_schedule();
            flags = spin_lock_irqsave(&m->guard);
        } else {
            /* Early boot or idle task: spin with pause hint */
            spin_unlock_irqrestore(&m->guard, flags);
            while (m->locked) {
                __asm__ volatile("pause");
            }
            flags = spin_lock_irqsave(&m->guard);
        }
    }
    m->locked = 1;
    spin_unlock_irqrestore(&m->guard, flags);
}

void mutex_unlock(mutex_t *m) {
    if (!m) return;
    uint64_t flags = spin_lock_irqsave(&m->guard);
    m->locked = 0;
    process_t *next = wait_queue_wake_one(&m->waiters);
    if (next) {
        next->state = PROC_READY;
        scheduler_add(next);
    }
    spin_unlock_irqrestore(&m->guard, flags);
}
