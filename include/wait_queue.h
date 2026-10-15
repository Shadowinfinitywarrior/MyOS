#ifndef WAIT_QUEUE_H
#define WAIT_QUEUE_H

#include "types.h"
#include "spinlock.h"

struct process;

typedef struct wait_queue_entry {
    struct process *task;
    struct wait_queue_entry *next;
} wait_queue_entry_t;

typedef struct wait_queue {
    wait_queue_entry_t *head;
    wait_queue_entry_t *tail;
} wait_queue_t;

#define WAIT_QUEUE_INIT { .head = NULL, .tail = NULL }

void wait_queue_init(wait_queue_t *wq);
void wait_queue_add(wait_queue_t *wq, struct process *task);
struct process *wait_queue_wake_one(wait_queue_t *wq);
void wait_queue_wake_all(wait_queue_t *wq);

#endif
