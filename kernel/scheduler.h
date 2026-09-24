#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "../include/system.h"

#include "process.h"

void scheduler_init(void);
void scheduler_start(void);
void scheduler_add(process_t *proc);
void scheduler_remove(process_t *proc);
void scheduler_schedule(void);
void scheduler_tick(void);      /* Called from timer IRQ */
void scheduler_wake_sleepers(void);

#endif

