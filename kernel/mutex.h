#ifndef MUTEX_H
#define MUTEX_H

#include "../include/types.h"
#include "../include/spinlock.h"
#include "../include/wait_queue.h"

typedef struct {
    spinlock_t guard;
    volatile int locked;
    wait_queue_t waiters;  /* processes sleeping on this mutex */
} mutex_t;

#define MUTEX_INIT { .guard = SPINLOCK_INIT, .locked = 0, .waiters = WAIT_QUEUE_INIT }

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
int  mutex_trylock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif
