#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"
#include "../include/system.h"
#include "paging.h"

#define MAX_PROCESSES    256
#define MAX_OPEN_FILES   16
#define PROCESS_NAME_LEN 32
#define KERNEL_STACK_SIZE 65536
#define USER_STACK_SIZE   32768
#define USER_STACK_TOP    0xBFFFF000

/* Process states */
typedef enum {
    PROC_UNUSED = 0,
    PROC_CREATED,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_SLEEPING,
    PROC_ZOMBIE,
    PROC_DEAD
} process_state_t;

/* CPU context saved during context switch */
typedef struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
    uint64_t padding;    /* 8 bytes padding so fpu starts at offset 96 */
    uint8_t  fpu[512] __attribute__((aligned(16)));
} __attribute__((aligned(16))) cpu_context_t;

/* Full register state for interrupt returns */
typedef struct trap_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t rip;
    uint64_t cs;
    uint64_t ss;
    uint64_t error_code;
    uint64_t int_no;
} trap_frame_t;

/* File descriptor */
typedef struct file_descriptor {
    struct vfs_node *node;
    uint32_t         offset;
    uint32_t         flags;
    int              in_use;
} file_descriptor_t;

/* Process Control Block */
typedef struct process {
    /* Identity */
    pid_t            pid;
    pid_t            ppid;
    char             name[PROCESS_NAME_LEN];
    process_state_t  state;

    /* CPU state */
    cpu_context_t    context;         /* Saved registers for switching */
    trap_frame_t    *trap_frame;      /* Saved user-mode registers */
    uint64_t         kernel_stack;    /* Top of kernel stack */
    uint64_t         kernel_stack_base;
    bool             is_user;         /* Ring-3: needs TSS/syscall stacks + VMA cleanup */

    /* Memory */
    page_directory_t *page_dir;
    uint64_t         heap_start;
    uint64_t         heap_end;

    /* Scheduling */
    uint32_t         priority;        /* 0 = highest */
    uint32_t         time_slice;      /* Remaining ticks */
    uint32_t         total_time;      /* Total CPU time used */
    uint32_t         sleep_until;     /* Wake-up tick count */

    /* File descriptors */
    file_descriptor_t fd_table[MAX_OPEN_FILES];

    /* Controlling virtual terminal. Console reads/writes for this process go
     * to this vty, which is either the boot console or a window's terminal. */
    struct vtty     *vtty;

    /* Signals */
    uint32_t         pending_signals;
    uint32_t         signal_mask;

    /* Exit */
    int              exit_code;

    /* wait(): pid being waited for (0 = none, -1 = any child) */
    pid_t            wait_child;

    /* Linked list pointers for scheduler queues */
    struct process  *next;
    struct process  *prev;
    int              in_queue;        /* Already enqueued on the ready list */
} process_t;

/* Process management API */
void       process_init(void);
process_t *process_create_kernel(const char *name, void (*entry)(void));
process_t *process_create_user(const char *name, const uint8_t *elf_data, uint64_t elf_size);
void       process_destroy(process_t *proc);
void       process_exit(int code);
void       process_yield(void);
void       process_sleep(uint32_t ms);
void       process_block(process_t *proc);
void       process_unblock(process_t *proc);
process_t *process_get_current(void);
process_t *process_get_by_pid(pid_t pid);
pid_t      process_fork(registers_t *frame);
int        process_exec(const char *path, const char **argv);
int        process_wait(pid_t pid, int *status);
int        process_kill(pid_t pid, int signal);
uint32_t   process_count(void);
void       process_dump_all(void);

/* File descriptor operations */
int        process_alloc_fd(process_t *proc);
void       process_free_fd(process_t *proc, int fd);

#endif

