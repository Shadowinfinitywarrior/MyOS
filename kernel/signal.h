#ifndef SIGNAL_H
#define SIGNAL_H

#include "../include/types.h"
#include "../include/system.h"
/* Forward declaration to break circular dependency */
struct process;
typedef struct process process_t;

/* Signal delivery - called when returning to user mode */
void signal_deliver(process_t *proc, struct registers *regs);

int  signal_send(pid_t pid, int signum);
void signal_init(void);

#endif