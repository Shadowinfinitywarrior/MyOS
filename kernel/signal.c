#include "signal.h"
#include "process.h"
#include "../lib/printf.h"
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

void signal_init(void) {
    kprintf("[SIGNAL] Signal subsystem initialized\n");
}

int signal_send(pid_t pid, int signum) {
    if (signum < 1 || signum >= NSIGNALS) return -1;

    process_t *proc = process_get_by_pid(pid);
    if (!proc) return -1;

    /* SIGKILL and SIGSTOP cannot be ignored */
    if (signum == SIGKILL) {
        kprintf("[SIGNAL] SIGKILL sent to PID %d\n", pid);
        process_exit(128 + signum);
        return 0;
    }

    /* Set pending signal bit */
    proc->pending_signals |= (1 << signum);

    /* Wake up if sleeping */
    if (proc->state == PROC_SLEEPING || proc->state == PROC_BLOCKED) {
        process_unblock(proc);
    }

    return 0;
}

