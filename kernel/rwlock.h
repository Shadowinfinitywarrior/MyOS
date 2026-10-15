#ifndef RWLOCK_H
#define RWLOCK_H

#include "../include/types.h"
#include "../include/spinlock.h"
#include "../include/wait_queue.h"

typedef struct {
    spinlock_t guard;
    int32_t readers;       /* >0 = readers active, -1 = writer active */
    wait_queue_t read_wait;
    wait_queue_t write_wait;
} rwlock_t;

#define RWLOCK_INIT { .guard = SPINLOCK_INIT, .readers = 0, .read_wait = WAIT_QUEUE_INIT, .write_wait = WAIT_QUEUE_INIT }

void rwlock_init(rwlock_t *r);
void rwlock_rdlock(rwlock_t *r);
void rwlock_rdunlock(rwlock_t *r);
void rwlock_wrlock(rwlock_t *r);
void rwlock_wrunlock(rwlock_t *r);

#endif
