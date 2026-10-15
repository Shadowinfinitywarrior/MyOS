#include "rwlock.h"
#include "process.h"
#include "scheduler.h"

void rwlock_init(rwlock_t *r) {
    if (!r) return;
    spinlock_init(&r->guard);
    r->readers = 0;
    wait_queue_init(&r->read_wait);
    wait_queue_init(&r->write_wait);
}

void rwlock_rdlock(rwlock_t *r) {
    if (!r) return;
    uint64_t flags = spin_lock_irqsave(&r->guard);
    while (r->readers < 0) { /* Writer active */
        process_t *curr = process_get_current();
        if (curr && curr->pid != 0) {
            curr->state = PROC_BLOCKED;
            wait_queue_add(&r->read_wait, curr);
            spin_unlock_irqrestore(&r->guard, flags);
            scheduler_schedule();
            flags = spin_lock_irqsave(&r->guard);
        } else {
            spin_unlock_irqrestore(&r->guard, flags);
            while (r->readers < 0) { __asm__ volatile("pause"); }
            flags = spin_lock_irqsave(&r->guard);
        }
    }
    r->readers++;
    spin_unlock_irqrestore(&r->guard, flags);
}

void rwlock_rdunlock(rwlock_t *r) {
    if (!r) return;
    uint64_t flags = spin_lock_irqsave(&r->guard);
    if (r->readers > 0) {
        r->readers--;
    }
    if (r->readers == 0) {
        process_t *writer = wait_queue_wake_one(&r->write_wait);
        if (writer) {
            writer->state = PROC_READY;
            scheduler_add(writer);
        }
    }
    spin_unlock_irqrestore(&r->guard, flags);
}

void rwlock_wrlock(rwlock_t *r) {
    if (!r) return;
    uint64_t flags = spin_lock_irqsave(&r->guard);
    while (r->readers != 0) {
        process_t *curr = process_get_current();
        if (curr && curr->pid != 0) {
            curr->state = PROC_BLOCKED;
            wait_queue_add(&r->write_wait, curr);
            spin_unlock_irqrestore(&r->guard, flags);
            scheduler_schedule();
            flags = spin_lock_irqsave(&r->guard);
        } else {
            spin_unlock_irqrestore(&r->guard, flags);
            while (r->readers != 0) { __asm__ volatile("pause"); }
            flags = spin_lock_irqsave(&r->guard);
        }
    }
    r->readers = -1;
    spin_unlock_irqrestore(&r->guard, flags);
}

void rwlock_wrunlock(rwlock_t *r) {
    if (!r) return;
    uint64_t flags = spin_lock_irqsave(&r->guard);
    r->readers = 0;
    process_t *writer = wait_queue_wake_one(&r->write_wait);
    if (writer) {
        writer->state = PROC_READY;
        scheduler_add(writer);
    } else {
        wait_queue_wake_all(&r->read_wait);
    }
    spin_unlock_irqrestore(&r->guard, flags);
}
