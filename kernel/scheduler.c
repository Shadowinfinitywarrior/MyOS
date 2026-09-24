#include "scheduler.h"
#include "timer.h"
#include "paging.h"
#include "tss.h"
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static process_t *ready_queue_head = NULL;
static int scheduler_enabled = 0;

extern void context_switch(cpu_context_t *old, cpu_context_t *new);
extern void syscall_set_kernel_stack(uintptr_t stack_top);
extern void process_set_current(process_t *proc);
extern process_t process_table[];

void scheduler_start(void) {
}

void scheduler_init(void) {
    ready_queue_head = NULL;
    scheduler_enabled = 1;
}

void scheduler_add(process_t *proc) {
    if (!proc || proc->state == PROC_UNUSED) return;
    const uint64_t flags = read_eflags();
    cli();
    if (proc->in_queue) {
        proc->state = PROC_READY;
        if (flags & 0x200) sti();
        return;
    }
    proc->state = PROC_READY;
    proc->in_queue = 1;
    if (!ready_queue_head) {
        ready_queue_head = proc;
        proc->next = proc;
        proc->prev = proc;
    } else {
        proc->next = ready_queue_head;
        proc->prev = ready_queue_head->prev;
        ready_queue_head->prev->next = proc;
        ready_queue_head->prev = proc;
    }
    if (flags & 0x200) sti();
}

void scheduler_remove(process_t *proc) {
    if (!proc || !proc->in_queue) return;
    const uint64_t flags = read_eflags();
    cli();
    if (proc->next == proc && proc->prev == proc) {
        if (ready_queue_head == proc) ready_queue_head = NULL;
    } else {
        proc->prev->next = proc->next;
        proc->next->prev = proc->prev;
        if (ready_queue_head == proc) ready_queue_head = proc->next;
    }
    proc->next = NULL;
    proc->prev = NULL;
    proc->in_queue = 0;
    if (flags & 0x200) sti();
}

void scheduler_schedule(void) {
    if (!scheduler_enabled) return;
    process_t *current = process_get_current();
    process_t *next = NULL;
    scheduler_wake_sleepers();
    if (ready_queue_head) {
        process_t *probe = ready_queue_head;
        int max_iterations = MAX_PROCESSES;
        while (max_iterations-- > 0) {
            if (probe->state == PROC_READY) {
                next = probe;
                break;
            }
            probe = probe->next;
            if (probe == ready_queue_head) break;
        }
        if (next) ready_queue_head = next->next;
    }
    if (!next) return;
    if (next == current) return;
    if (current && current->state == PROC_RUNNING) {
        current->state = PROC_READY;
        scheduler_add(current);
    }
    next->state = PROC_RUNNING;
    next->time_slice = 10;
    process_set_current(next);
    /* Keep the per-CPU ring-0 stacks honest: the TSS rsp0 (used when a
     * Ring-3 interrupt pushes a frame) and the SYSCALL kernel stack must
     * both point at this process's kernel stack while it is scheduled. */
    if (next->kernel_stack) {
        tss_set_rsp0(next->kernel_stack);
        syscall_set_kernel_stack(next->kernel_stack);
    }
    if (next->page_dir && next->page_dir != paging_get_directory()) {
        paging_switch_directory(next->page_dir);
    }
    if (current) {
        context_switch(&current->context, &next->context);
    } else {
        context_switch(NULL, &next->context);
    }
}

void scheduler_tick(void) {
    process_t *current = process_get_current();
    if (!current || !scheduler_enabled) return;
    current->total_time++;
    scheduler_wake_sleepers();
    if (current->time_slice > 0) current->time_slice--;
    if (current->time_slice == 0) scheduler_schedule();
}

void scheduler_wake_sleepers(void) {
    uint64_t now = timer_get_ticks();
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &process_table[i];
        if (p->state == PROC_SLEEPING && (uint64_t)p->sleep_until <= now) {
            p->state = PROC_READY;
            scheduler_add(p);
        }
    }
}
