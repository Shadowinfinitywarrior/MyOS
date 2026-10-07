#include "signal.h"
#include "process.h"
#include "syscall.h"
#include "paging.h"
#include "scheduler.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern process_t process_table[MAX_PROCESSES];

void signal_init(void) {
    kprintf("[SIGNAL] Signal subsystem initialized\n");
}

int signal_send(pid_t pid, int signum) {
    if (signum < 1 || signum >= NSIGNALS) return -1;

    process_t *proc = process_get_by_pid(pid);
    if (!proc) return -1;

    /* SIGKILL and SIGSTOP cannot be ignored */
    if (signum == SIGKILL || signum == SIGSTOP) {
        if (signum == SIGKILL) {
            kprintf("[SIGNAL] SIGKILL sent to PID %d\n", pid);
            process_exit(128 + signum);
        } else {
            proc->state = PROC_BLOCKED;
            scheduler_remove(proc);
        }
        return 0;
    }

    /* Check if signal is ignored */
    if (proc->sig_actions[signum].sa_handler.sa_handler == SIG_IGN)
        return 0;

    /* Check if signal is blocked */
    if (proc->signal_mask & (1u << signum)) {
        proc->pending_signals |= (1u << signum);
        return 0;
    }

    /* Set pending signal bit */
    proc->pending_signals |= (1u << signum);

    /* Wake up if sleeping */
    if (proc->state == PROC_SLEEPING || proc->state == PROC_BLOCKED) {
        process_unblock(proc);
    }

    return 0;
}

/* Deliver pending signals to a process when returning to user mode */
void signal_deliver(process_t *proc, registers_t *regs) {
    if (!proc || !proc->is_user) return;

    uint32_t pending = proc->pending_signals & ~proc->signal_mask;
    if (!pending) return;

    /* Find first pending signal */
    int signum = 0;
    while (signum < NSIGNALS && !(pending & (1u << signum)))
        signum++;

    if (signum >= NSIGNALS) return;

    /* Clear the pending bit */
    proc->pending_signals &= ~(1u << signum);

    struct sigaction *action = &proc->sig_actions[signum];

    /* Default action */
    if (action->sa_handler.sa_handler == SIG_DFL) {
        switch (signum) {
            case SIGKILL:
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
            case SIGABRT:
                kprintf("[SIGNAL] Default action: terminate PID %d (signal %d)\n", proc->pid, signum);
                process_exit(128 + signum);
                break;
            case SIGSTOP:
            case SIGTSTP:
            case SIGTTIN:
            case SIGTTOU:
                proc->state = PROC_BLOCKED;
                scheduler_remove(proc);
                break;
            case SIGCONT:
                if (proc->state == PROC_BLOCKED)
                    process_unblock(proc);
                break;
            case SIGCHLD:
            case SIGWINCH:
            case SIGURG:
                /* Ignore by default */
                break;
            default:
                kprintf("[SIGNAL] Default action: terminate PID %d (signal %d)\n", proc->pid, signum);
                process_exit(128 + signum);
                break;
        }
        return;
    }

    /* Custom handler */
    sighandler_t handler = action->sa_handler.sa_handler;
    if (!handler) return;

    /* Prepare signal frame on user stack */
    uint64_t user_rsp = regs->rsp;
    user_rsp -= 128; /* Red zone + alignment */

    /* Build siginfo_t on user stack */
    siginfo_t *si = (siginfo_t *)(uintptr_t)user_rsp;
    page_directory_t *old_dir = paging_get_active();
    if (proc->page_dir) paging_switch_directory(proc->page_dir);

    si->si_signo = signum;
    si->si_code = 0;  /* SI_USER */
    si->si_errno = 0;
    si->si_pid = proc->ppid;
    si->si_uid = 0;
    si->si_addr = NULL;
    si->si_status = 0;
    si->si_band = 0;

    /* Save current register state for sigreturn */
    uint64_t *saved_regs = (uint64_t *)(uintptr_t)(user_rsp - sizeof(registers_t));
    memcpy(saved_regs, regs, sizeof(registers_t));

    /* Set up return address to sigreturn trampoline */
    extern void sigreturn_trampoline(void);
    regs->rip = (uint64_t)sigreturn_trampoline;
    regs->rsp = (uint64_t)saved_regs;
    regs->rdi = signum;
    regs->rsi = (uint64_t)si;
    regs->rdx = (uint64_t)saved_regs;

    /* If SA_SIGINFO, pass siginfo_t and ucontext_t */
    if (action->sa_flags & SA_SIGINFO) {
        regs->rsi = (uint64_t)si;
        regs->rdx = (uint64_t)saved_regs;
    }

    /* Add signal to mask if SA_NODEFER not set */
    if (!(action->sa_flags & SA_NODEFER)) {
        /* Save old mask in saved_regs (reuse r11 slot which holds user RFLAGS in syscall frame) */
        *(uint32_t *)((uint8_t *)saved_regs + offsetof(registers_t, r11)) = proc->signal_mask;
        proc->signal_mask |= (1u << signum);
    } else {
        /* Save current mask for restoration */
        *(uint32_t *)((uint8_t *)saved_regs + offsetof(registers_t, r11)) = proc->signal_mask;
    }

    /* Reset handler if SA_RESETHAND */
    if (action->sa_flags & SA_RESETHAND) {
        action->sa_handler.sa_handler = SIG_DFL;
    }

    paging_switch_directory(old_dir);
}

/* sigaction syscall: change signal action */
int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (signum < 1 || signum >= NSIGNALS) return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL; /* Cannot change */

    process_t *proc = process_get_current();
    if (!proc) return -ESRCH;

    struct sigaction *current = &proc->sig_actions[signum];

    if (oldact) {
        page_directory_t *old_dir = paging_get_active();
        if (proc->page_dir) paging_switch_directory(proc->page_dir);
        memcpy((void *)oldact, current, sizeof(struct sigaction));
        paging_switch_directory(old_dir);
    }

    if (act) {
        page_directory_t *old_dir = paging_get_active();
        if (proc->page_dir) paging_switch_directory(proc->page_dir);
        memcpy(current, (const void *)act, sizeof(struct sigaction));
        paging_switch_directory(old_dir);
    }

    return 0;
}

/* sigprocmask syscall: examine/change signal mask */
int sys_sigprocmask(int how, const uint32_t *set, uint32_t *oldset) {
    process_t *proc = process_get_current();
    if (!proc) return -ESRCH;

    if (oldset) {
        page_directory_t *old_dir = paging_get_active();
        if (proc->page_dir) paging_switch_directory(proc->page_dir);
        *(uint32_t *)oldset = proc->signal_mask;
        paging_switch_directory(old_dir);
    }

    if (set) {
        uint32_t new_mask;
        page_directory_t *old_dir = paging_get_active();
        if (proc->page_dir) paging_switch_directory(proc->page_dir);
        new_mask = *set;
        paging_switch_directory(old_dir);

        switch (how) {
            case SIG_BLOCK:
                proc->signal_mask |= new_mask;
                break;
            case SIG_UNBLOCK:
                proc->signal_mask &= ~new_mask;
                break;
            case SIG_SETMASK:
                proc->signal_mask = new_mask;
                break;
            default:
                return -EINVAL;
        }
        /* SIGKILL and SIGSTOP cannot be blocked */
        proc->signal_mask &= ~((1u << SIGKILL) | (1u << SIGSTOP));
    }

    return 0;
}

/* sigreturn syscall: return from signal handler */
int sys_sigreturn(registers_t *saved_regs) {
    process_t *proc = process_get_current();
    if (!proc) return -ESRCH;

    /* Verify the saved_regs pointer is in user space */
    if ((uintptr_t)saved_regs < 0x40000000 || (uintptr_t)saved_regs >= 0xFFFF800000000000ULL) {
        return -EFAULT;
    }

    /* Get the current syscall frame (on kernel stack) */
    extern registers_t *g_current_regs;
    registers_t *current_frame = g_current_regs;
    if (!current_frame) return -EFAULT;

    /* Restore signal mask from saved frame (stored in r11 slot) */
    page_directory_t *old_dir = paging_get_active();
    if (proc->page_dir) paging_switch_directory(proc->page_dir);
    proc->signal_mask = *(uint32_t *)((uint8_t *)saved_regs + offsetof(registers_t, r11));
    paging_switch_directory(old_dir);

    /* Copy all registers from saved frame to current frame */
    if (proc->page_dir) paging_switch_directory(proc->page_dir);
    memcpy(current_frame, saved_regs, sizeof(registers_t));
    paging_switch_directory(old_dir);

    return 0;
}