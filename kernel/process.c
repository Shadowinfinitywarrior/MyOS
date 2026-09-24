#include "process.h"
#include "scheduler.h"
#include "paging.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "elf.h"
#include "tss.h"
#include "syscall.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../fs/vfs.h"

#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

extern void syscall_set_kernel_stack(uintptr_t stack_top);

process_t process_table[MAX_PROCESSES];
static process_t *current_process = NULL;
static pid_t next_pid = 1;

/* Forward declarations */
extern void context_switch(cpu_context_t *old, cpu_context_t *new);
extern void usermode_enter_trampoline(void);
extern void fork_child_iret(void);

static void process_wake_waiters(process_t *child);

void process_init(void) {
    memset(process_table, 0, sizeof(process_table));
    next_pid = 1;

    /* Create process 0 (idle/kernel process) */
    process_t *idle = &process_table[0];
    idle->pid = 0;
    idle->ppid = 0;
    strcpy(idle->name, "idle");
    idle->state = PROC_RUNNING;
    idle->page_dir = paging_get_directory();
    idle->priority = 255;    /* Lowest priority */
    idle->time_slice = 1;

    current_process = idle;

    kprintf("[PROC] Process subsystem initialized (PID 0 = idle)\n");
}

static process_t *alloc_process(void) {
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            memset(&process_table[i], 0, sizeof(process_t));
            return &process_table[i];
        }
    }
    return NULL;
}

process_t *process_create_kernel(const char *name, void (*entry)(void)) {
    process_t *proc = alloc_process();
    if (!proc) {
        kprintf("[PROC] Error: No free process slots!\n");
        return NULL;
    }

    proc->pid = next_pid++;
    proc->ppid = current_process ? current_process->pid : 0;
    strncpy(proc->name, name, PROCESS_NAME_LEN - 1);
    proc->state = PROC_CREATED;
    proc->priority = 10;
    proc->time_slice = 10;

    /* Allocate kernel stack (16 pages for 64KB) contiguous */
    uint64_t stack_pages[16];
    bool stack_done = false;
    while (!stack_done) {
        uint64_t base = pmm_alloc_page();
        if (!base) {
            kprintf("[PROC] Failed to allocate kernel stack base page\n");
            return NULL;
        }
        stack_pages[0] = base;
        int count = 1;
        bool ok = true;
        for (int i = 1; i < 16; i++) {
            uint64_t p = pmm_alloc_page();
            if (!p) { ok = false; break; }
            stack_pages[i] = p;
            if (p != base + i * PAGE_SIZE) { ok = false; break; }
            count++;
        }
        if (ok && count == 16) {
            proc->kernel_stack_base = base;
            stack_done = true;
        } else {
            /* free only pages actually allocated */
            for (int j = 0; j < count; j++) {
                pmm_free_page(stack_pages[j]);
            }
            /* also free any partially allocated extra pages if ok failed after allocation */
            /* count already accounts for successfully checked pages */
            continue;
        }
    }
    /* Zero stack memory */
    memset((void*)(uintptr_t)proc->kernel_stack_base, 0, KERNEL_STACK_SIZE);
    proc->kernel_stack = proc->kernel_stack_base + KERNEL_STACK_SIZE;
    proc->kernel_stack -= 8; /* ABI alignment: RSP % 16 == 8 at entry */
    kprintf("[PROC] kernel stack base=0x%llx top=0x%llx\n", (unsigned long long)proc->kernel_stack_base, (unsigned long long)proc->kernel_stack);

    /* Set up initial context for first switch */
    proc->context.rsp = proc->kernel_stack;
    proc->context.rbp = proc->kernel_stack;
    proc->context.rip = (uint64_t)entry;
    proc->context.rflags = 0x202;
    memset(proc->context.fpu, 0, sizeof(proc->context.fpu));

    /* Use kernel page directory */
    proc->page_dir = paging_get_directory();

    /* Initialize file descriptors */
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        proc->fd_table[i].in_use = 0;

    kprintf("[PROC] about to call vfs_resolve_path\n");
    /* Open stdin/stdout/stderr (FDs 0, 1, 2) */
    vfs_node_t *console = vfs_resolve_path("/dev/console");
    kprintf("[PROC] vfs_resolve_path returned 0x%lx\n", (uint64_t)console);
    if (console) {
        for (int i = 0; i < 3; i++) {
            proc->fd_table[i].in_use = 1;
            proc->fd_table[i].node = console;
            proc->fd_table[i].offset = 0;
            proc->fd_table[i].flags = 0;
        }
    }

    proc->state = PROC_READY;
    scheduler_add(proc);

    kprintf("[PROC] Created kernel process '%s' (PID %d)\n", name, proc->pid);
    return proc;
}

process_t *process_create_user(const char *name, const uint8_t *elf_data, uint64_t elf_size) {
    process_t *proc = alloc_process();
    if (!proc) return NULL;

    proc->pid = next_pid++;
    proc->ppid = current_process ? current_process->pid : 0;
    strncpy(proc->name, name, PROCESS_NAME_LEN - 1);
    proc->state = PROC_CREATED;
    proc->priority = 20;
    proc->time_slice = 20;

    proc->page_dir = paging_clone_directory(paging_get_directory());
    if (!proc->page_dir) {
        kprintf("[PROC] Failed to clone page directory\n");
        return NULL;
    }

    /* Allocate kernel stack (16 pages for 64KB) contiguous */
    uint64_t stack_pages[16];
    bool stack_done = false;
    while (!stack_done) {
        uint64_t base = pmm_alloc_page();
        if (!base) {
            kprintf("[PROC] Failed to allocate kernel stack base page (user)\n");
            return NULL;
        }
        stack_pages[0] = base;
        int count = 1;
        bool ok = true;
        for (int i = 1; i < 16; i++) {
            uint64_t p = pmm_alloc_page();
            if (!p) { ok = false; break; }
            stack_pages[i] = p;
            if (p != base + i * PAGE_SIZE) { ok = false; break; }
            count++;
        }
        if (ok && count == 16) {
            proc->kernel_stack_base = base;
            stack_done = true;
        } else {
            for (int j = 0; j < count; j++) {
                pmm_free_page(stack_pages[j]);
            }
            continue;
        }
    }
    memset((void*)(uintptr_t)proc->kernel_stack_base, 0, KERNEL_STACK_SIZE);
    proc->kernel_stack = proc->kernel_stack_base + KERNEL_STACK_SIZE;
    proc->kernel_stack -= 8;
    proc->context.rsp = proc->kernel_stack;
    proc->context.rbp = proc->kernel_stack;
    proc->context.rip = (uint64_t)usermode_enter_trampoline;
    proc->context.rflags = 0x202;
    memset(proc->context.fpu, 0, sizeof(proc->context.fpu));

    uint64_t user_stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
    page_directory_t *old_dir = paging_get_active();
    paging_switch_directory(proc->page_dir);
    for (uint64_t addr = user_stack_bottom; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            kprintf("[PROC] Failed to allocate user stack page\n");
            paging_switch_directory(old_dir);
            process_destroy(proc);
            return NULL;
        }
        paging_map(addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    paging_switch_directory(old_dir);

    uint64_t entry_point = elf_load(proc->page_dir, elf_data, elf_size);
    if (entry_point == 0) {
        kprintf("[PROC] Failed to load ELF for '%s'\n", name);
        process_destroy(proc);
        return NULL;
    }

    /* First ring-3 entry is delivered by usermode_enter_trampoline:
     * rdi = user entry point, rsi = user RSP.  After that the process is
     * saved/resumed through ordinary interrupt frames. */
    proc->is_user = 1;
    proc->context.rdi = entry_point;
    proc->context.rsi = USER_STACK_TOP - 8;

    for (int i = 0; i < MAX_OPEN_FILES; i++)
        proc->fd_table[i].in_use = 0;

    vfs_node_t *console = vfs_resolve_path("/dev/console");
    if (console) {
        for (int i = 0; i < 3; i++) {
            proc->fd_table[i].in_use = 1;
            proc->fd_table[i].node = console;
            proc->fd_table[i].offset = 0;
        }
    }

    proc->state = PROC_READY;
    scheduler_add(proc);

    kprintf("[PROC] Created user process '%s' (PID %d, entry=0x%08X)\n", name, proc->pid, entry_point);
    return proc;
}

void process_destroy(process_t *proc) {
    if (!proc || proc->pid == 0) return;

    cli();
    proc->state = PROC_DEAD;
    scheduler_remove(proc);

    if (proc->kernel_stack_base) {
        for (int i = 0; i < 16; i++) {
            uint64_t page = proc->kernel_stack_base + i * PAGE_SIZE;
            if (page) pmm_free_page(page);
        }
    }
    if (proc->page_dir != paging_get_directory()) {
        page_directory_t *old_dir = paging_get_active();
        paging_switch_directory(proc->page_dir);
        /* Free every frame owned by this address space: the ELF image VMA
         * and the user stack.  Kernel identity-mapped pages are shared and
         * must NOT be released here. */
        for (uint64_t addr = ELF_USER_VMA_MIN; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
            uint64_t phys = paging_get_physical(addr);
            if (phys) {
                pmm_free_page(phys);
                paging_unmap(addr);
            }
        }
        paging_switch_directory(old_dir);
    }
    proc->state = PROC_UNUSED;
    sti();
}

void process_exit(int code) {
    process_t *proc = current_process;
    if (!proc || proc->pid == 0) {
        PANIC("Idle process tried to exit!");
        return;
    }

    kprintf("[PROC] Process '%s' (PID %d) exited with code %d\n",
            proc->name, proc->pid, code);

    cli();
    proc->exit_code = code;
    proc->state = PROC_ZOMBIE;

    /* Reparent children to init (PID 1) */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_UNUSED &&
            process_table[i].ppid == proc->pid) {
            process_table[i].ppid = 1;
        }
    }

    process_wake_waiters(proc);

    scheduler_schedule();
    /* Should not return */
    hang();
}

void process_yield(void) {
    cli();
    if (current_process)
        current_process->state = PROC_READY;
    scheduler_schedule();
    sti();
}

void process_sleep(uint32_t ms) {
    if (!current_process || current_process->pid == 0) {
        /* Kernel idle: just busy-wait with HLT */
        uint64_t target = timer_get_ticks() + ms;
        while (timer_get_ticks() < target)
            hlt();
        return;
    }

    cli();
    current_process->state = PROC_SLEEPING;
    current_process->sleep_until = (uint32_t)timer_get_ticks() + ms;
    scheduler_schedule();
    sti();
}

void process_block(process_t *proc) {
    cli();
    proc->state = PROC_BLOCKED;
    if (proc == current_process)
        scheduler_schedule();
    sti();
}

void process_unblock(process_t *proc) {
    cli();
    if (proc->state == PROC_BLOCKED || proc->state == PROC_SLEEPING) {
        proc->state = PROC_READY;
        scheduler_add(proc);
    }
    sti();
}

process_t *process_get_current(void) {
    return current_process;
}

void process_set_current(process_t *proc) {
    current_process = proc;
}

process_t *process_get_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid &&
            process_table[i].state != PROC_UNUSED)
            return &process_table[i];
    }
    return NULL;
}

pid_t process_fork(registers_t *frame) {
    /* Ring-3 fork: duplicate the parent's address space and resume the child
     * at the instruction following its SYSCALL (returning 0 from fork()). */
    process_t *parent = process_get_current();
    if (!parent || !parent->is_user || !frame) {
        kprintf("[PROC] fork: requires a Ring-3 parent with a syscall frame\n");
        return -1;
    }

    process_t *child = alloc_process();
    if (!child) {
        kprintf("[PROC] fork: no free process slots\n");
        return -1;
    }

    child->pid = next_pid++;
    child->ppid = parent->pid;
    strncpy(child->name, parent->name, PROCESS_NAME_LEN - 1);
    child->name[PROCESS_NAME_LEN - 1] = '\0';
    child->state = PROC_CREATED;
    child->priority = parent->priority;
    child->time_slice = parent->time_slice;
    child->is_user = 1;
    child->wait_child = 0;
    child->exit_code = 0;

    child->page_dir = paging_clone_directory(parent->page_dir);
    if (!child->page_dir) {
        kprintf("[PROC] fork: failed to clone page directory\n");
        child->state = PROC_UNUSED;
        return -1;
    }

    /* Allocate kernel stack (16 pages for 64KB) contiguous */
    uint64_t stack_pages[16];
    bool stack_done = false;
    while (!stack_done) {
        uint64_t base = pmm_alloc_page();
        if (!base) {
            kprintf("[PROC] fork: failed to allocate kernel stack base\n");
            child->state = PROC_UNUSED;
            return -1;
        }
        stack_pages[0] = base;
        int count = 1;
        bool ok = true;
        for (int i = 1; i < 16; i++) {
            uint64_t p = pmm_alloc_page();
            if (!p) { ok = false; break; }
            stack_pages[i] = p;
            if (p != base + i * PAGE_SIZE) { ok = false; break; }
            count++;
        }
        if (ok && count == 16) {
            child->kernel_stack_base = base;
            stack_done = true;
        } else {
            for (int j = 0; j < count; j++) pmm_free_page(stack_pages[j]);
            continue;
        }
    }
    child->kernel_stack = child->kernel_stack_base + KERNEL_STACK_SIZE;
    child->kernel_stack -= 8;

    /* Copy the user address space with eager per-page semantics: every
     * mapped page in the parent band gets a fresh frame with the parent's
     * contents, so parent and child no longer share writable memory. */
    page_directory_t *old_dir = paging_get_active();
    paging_switch_directory(child->page_dir);
    bool copy_ok = true;
    for (uint64_t addr = ELF_USER_VMA_MIN; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        uint64_t phys = paging_get_physical(addr);
        if (!phys) continue;
        uint64_t newphys = pmm_alloc_page();
        if (!newphys) { copy_ok = false; break; }
        paging_map(addr, newphys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        memcpy((void *)(uintptr_t)addr, (const void *)(uintptr_t)phys, PAGE_SIZE);
    }
    paging_switch_directory(old_dir);
    if (!copy_ok) {
        kprintf("[PROC] fork: out of memory while copying address space\n");
        process_destroy(child);
        return -1;
    }

    /* Copy the file descriptor table. */
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        child->fd_table[i] = parent->fd_table[i];

    /* The child resumes in Ring 3 through fork_child_iret with a copied
     * registers_t frame (rax forced to 0 = "child side of fork"). */
    registers_t *cframe = (registers_t *)(child->kernel_stack - sizeof(registers_t));
    memcpy(cframe, frame, sizeof(registers_t));
    cframe->rax = 0;
    cframe->int_no = 0;
    cframe->err_code = 0;
    child->context.rsp = (uint64_t)cframe;
    child->context.rip = (uint64_t)fork_child_iret;
    child->context.rflags = 0x202;
    memset(child->context.fpu, 0, sizeof(child->context.fpu));

    child->state = PROC_READY;
    scheduler_add(child);
    kprintf("[PROC] fork: '%s' (PID %d) -> child PID %d\n",
            parent->name, parent->pid, child->pid);
    return child->pid;
}

/* Wake any parents blocked in process_wait for the given child. */
static void process_wake_waiters(process_t *child) {
    if (!child) return;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &process_table[i];
        if (p->state != PROC_BLOCKED) continue;
        if (p->wait_child == child->pid || p->wait_child == (pid_t)-1)
            process_unblock(p);
    }
}

int process_wait(pid_t pid, int *status) {
    process_t *parent = process_get_current();
    if (!parent || parent->pid == 0) return -1;

    for (;;) {
        bool found_live = false;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            process_t *p = &process_table[i];
            if (p->state == PROC_UNUSED) continue;
            if (p->ppid != parent->pid) continue;
            if (pid >= 0 && p->pid != pid) continue;

            if (p->state == PROC_ZOMBIE) {
                int code = p->exit_code;
                pid_t reaped = p->pid;
                process_destroy(p);
                kprintf("[PROC] wait: reaped child PID %d (code %d)\n", reaped, code);
                if (status) *status = code;
                return reaped;
            }
            found_live = true;
        }

        if (!found_live) return -ECHILD;

        /* A matching child is alive: block until it dies. process_exit /
         * process_kill wake us via process_wake_waiters. */
        cli();
        parent->wait_child = pid;
        parent->state = PROC_BLOCKED;
        scheduler_schedule();
        parent->wait_child = 0;
        sti();
    }
}

int process_kill(pid_t pid, int signal) {
    if (signal != 9) return -EINVAL;   /* Only SIGKILL supported for now */

    cli();
    process_t *proc = process_get_by_pid(pid);
    if (!proc) {
        sti();
        return -ESRCH;
    }
    if (proc->state == PROC_ZOMBIE || proc->state == PROC_UNUSED) {
        sti();
        return -ESRCH;                 /* Already dead/being reaped */
    }

    proc->exit_code = 128 + signal;

    if (proc == process_get_current()) {
        /* Suicide: mirror process_exit (never returns). */
        proc->state = PROC_ZOMBIE;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state != PROC_UNUSED &&
                process_table[i].ppid == proc->pid)
                process_table[i].ppid = 1;
        }
        process_wake_waiters(proc);
        scheduler_schedule();
        hang();
    }

    proc->state = PROC_ZOMBIE;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_UNUSED &&
            process_table[i].ppid == proc->pid)
            process_table[i].ppid = 1;
    }
    process_wake_waiters(proc);
    sti();
    kprintf("[PROC] kill: PID %d (signal %d)\n", pid, signal);
    return 0;
}

int process_exec(const char *path, const char **argv) {
    /* The runtime-unsafe half of exec (discarding the current address space
     * mid-frame). sys_execve keeps its spawn-a-new-process semantics, see
     * syscall.c. */
    (void)path; (void)argv;
    return -ENOSYS;
}

int process_alloc_fd(process_t *proc) {
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!proc->fd_table[i].in_use) {
            proc->fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

void process_free_fd(process_t *proc, int fd) {
    if (fd >= 0 && fd < MAX_OPEN_FILES) {
        proc->fd_table[fd].in_use = 0;
        proc->fd_table[fd].node = NULL;
    }
}

uint32_t process_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_UNUSED)
            count++;
    }
    return count;
}

void process_dump_all(void) {
    kprintf("\n  PID  PPID  STATE      PRIO  NAME\n");
    kprintf("  ---  ----  ---------  ----  ----\n");

    const char *state_names[] = {
        "UNUSED", "CREATED", "READY", "RUNNING",
        "BLOCKED", "SLEEPING", "ZOMBIE", "DEAD"
    };

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &process_table[i];
        if (p->state == PROC_UNUSED) continue;

        kprintf("  %-4d %-5d %-10s %-5d %s%s\n",
                p->pid, p->ppid,
                state_names[p->state],
                p->priority,
                p->name,
                (p == current_process) ? " *" : "");
    }
    kprintf("\n");
}

