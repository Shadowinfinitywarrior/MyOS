#include "../include/wait_queue.h"
#include "heap.h"
#include "process.h"
#include "../lib/string.h"

void wait_queue_init(wait_queue_t *wq) {
    if (!wq) return;
    wq->head = NULL;
    wq->tail = NULL;
}

void wait_queue_add(wait_queue_t *wq, struct process *task) {
    if (!wq || !task) return;
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    if (!entry) return;
    entry->task = task;
    entry->next = NULL;
    if (!wq->tail) {
        wq->head = entry;
        wq->tail = entry;
    } else {
        wq->tail->next = entry;
        wq->tail = entry;
    }
}

struct process *wait_queue_wake_one(wait_queue_t *wq) {
    if (!wq || !wq->head) return NULL;
    wait_queue_entry_t *entry = wq->head;
    wq->head = entry->next;
    if (!wq->head) {
        wq->tail = NULL;
    }
    struct process *task = entry->task;
    kfree(entry);
    return task;
}

void wait_queue_wake_all(wait_queue_t *wq) {
    while (wait_queue_wake_one(wq)) {}
}
