# MyOS System Call ABI Reference

## Overview

MyOS uses the **x86-64 `SYSCALL`/`SYSRET`** instruction pair for fast transitions between user mode (Ring 3) and kernel mode (Ring 0). The calling convention follows the **System V AMD64 ABI** for argument passing.

---

## Calling Convention

### Register Usage (SysV AMD64)

| Register | Purpose | Preserved Across Syscall |
|----------|---------|--------------------------|
| `RAX`    | Syscall number (input) / Return value (output) | No |
| `RDI`    | Argument 1 | No |
| `RSI`    | Argument 2 | No |
| `RDX`    | Argument 3 | No |
| `R10`    | Argument 4 | No |
| `R8`     | Argument 5 | No |
| `R9`     | Argument 6 | No |
| `RCX`    | User RIP (saved by `SYSCALL`) | No |
| `R11`    | User RFLAGS (saved by `SYSCALL`) | No |
| `RBX`    | Callee-saved | Yes |
| `RBP`    | Callee-saved | Yes |
| `R12–R15`| Callee-saved | Yes |
| `RSP`    | User stack pointer | No (restored on return) |

> **Note:** `R10` is used instead of `RCX` for argument 4 because `SYSCALL` clobbers `RCX` (stores user RIP). `R11` is clobbered (stores user RFLAGS).

### Return Values

- On success: `RAX` ≥ 0 (syscall-specific return value)
- On error: `RAX` ∈ [−4095, −1] (negative `errno`)

User-space libc converts negative returns to `−1` and sets `errno = -RAX`.

---

## Segment Selectors

| Selector | Value | Description |
|----------|-------|-------------|
| Kernel Code (CS) | `0x08` | GDT index 1, RPL 0 |
| Kernel Data (SS/DS/ES/FS/GS) | `0x10` | GDT index 2, RPL 0 |
| User Code (CS) | `0x23` | GDT index 4, RPL 3 |
| User Data (SS/DS/ES/FS/GS) | `0x1B` | GDT index 3, RPL 3 |

The `STAR` MSR is programmed as:
- `STAR[47:32]` = `0x08` (kernel CS for `SYSCALL`)
- `STAR[63:48]` = `0x13` (base for `SYSRET`: CS = base+16 = `0x23`, SS = base+8 = `0x1B`)

---

## Syscall Numbers

| Number | Name | Arguments | Description |
|--------|------|-----------|-------------|
| 1 | `SYS_EXIT` | `int code` | Terminate current process |
| 2 | `SYS_FORK` | — | Create child process (returns 0 in child, PID in parent) |
| 3 | `SYS_READ` | `int fd, void *buf, size_t count` | Read from file descriptor |
| 4 | `SYS_WRITE` | `int fd, const void *buf, size_t count` | Write to file descriptor |
| 5 | `SYS_OPEN` | `const char *path, int flags` | Open file, return fd |
| 6 | `SYS_CLOSE` | `int fd` | Close file descriptor |
| 7 | `SYS_WAIT` | `pid_t pid, int *status` | Wait for child process |
| 8 | `SYS_EXEC` | `const char *path` | Execute program (legacy) |
| 9 | `SYS_GETPID` | — | Get current process ID |
| 10 | `SYS_SLEEP` | `uint32_t ms` | Sleep for milliseconds |
| 11 | `SYS_YIELD` | — | Yield CPU to scheduler |
| 12 | `SYS_KILL` | `pid_t pid, int sig` | Send signal to process |
| 13 | `SYS_BRK` | `void *addr` | Change heap break (not implemented) |
| 14 | `SYS_MMAP` | `void *addr, size_t len, int prot, int flags, int fd, off_t off` | Map memory region |
| 15 | `SYS_MUNMAP` | `void *addr, size_t len` | Unmap memory region |
| 16 | `SYS_MPROTECT` | `void *addr, size_t len, int prot` | Change memory protection |
| 17 | `SYS_GETCWD` | `char *buf, size_t size` | Get working directory (not implemented) |
| 18 | `SYS_CHDIR` | `const char *path` | Change directory (not implemented) |
| 19 | `SYS_MKDIR` | `const char *path, int mode` | Create directory (not implemented) |
| 20 | `SYS_UNLINK` | `const char *path` | Remove file (not implemented) |
| 21 | `SYS_SHMGET` | `const char *name, size_t size, int flags` | Create/open shared memory |
| 22 | `SYS_SHMCTL` | `int fd, int cmd, void *arg` | Control shared memory |
| 23 | `SYS_TIME` | — | Get current time (seconds) |
| 24 | `SYS_GETCHAR` | — | Read character from console |
| 25 | `SYS_PUTCHAR` | `char c` | Write character to console |
| 26 | `SYS_PS` | — | List processes |
| 27 | `SYS_UPTIME` | — | Get system uptime (seconds) |
| 28 | `SYS_EXECVE` | `const char *path` | Execute program (replaces process) |
| 29 | `SYS_REBOOT` | — | Reboot system |
| 30 | `SYS_SHUTDOWN` | — | Power off system |
| 31 | `SYS_MEMINFO` | — | Print memory info |
| 32 | `SYS_READDIR` | `int fd, int index, char *name` | Read directory entry (not implemented) |
| 40 | `SYS_GUI_CREATE_SURFACE` | `int width, int height` | Create GUI surface |
| 41 | `SYS_GUI_BLIT_SURFACE` | `void *surface, int x, int y` | Blit surface to screen |
| 42 | `SYS_GUI_INVALIDATE` | `uint64_t id, int x, int y, int w, int h` | Invalidate window region |
| 43 | `SYS_GUI_GET_FB_INFO` | `int *width, int *height, int *pitch` | Get framebuffer info |

---

## Error Codes (`errno`)

| Value | Name | Description |
|-------|------|-------------|
| 1 | `EPERM` | Operation not permitted |
| 2 | `ENOENT` | No such file or directory |
| 3 | `ESRCH` | No such process |
| 9 | `EBADF` | Bad file descriptor |
| 10 | `ECHILD` | No child processes |
| 11 | `EAGAIN` | Resource temporarily unavailable |
| 14 | `EFAULT` | Bad address |
| 22 | `EINVAL` | Invalid argument |
| 38 | `ENOSYS` | Function not implemented |

---

## User-Space Wrapper (libc)

```c
/* In user/libc.c */
long _syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    if (ret < 0 && ret >= -4096) {
        errno = (int)(-ret);
        return -1;
    }
    return ret;
}
```

Example usage:
```c
pid_t fork(void) {
    return _syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

int read(int fd, void *buf, size_t count) {
    long ret = _syscall(SYS_READ, fd, (long)buf, (long)count, 0, 0, 0);
    return (int)ret;
}
```

---

## Kernel Entry Point (`kernel/context_switch.asm`)

```asm
global syscall_entry64
syscall_entry64:
    ; Save clobbered registers (RCX=RIP, R11=RFLAGS, RAX=num, RSP=user stack)
    mov  [syscall_cpu + 8],  r9
    mov  [syscall_cpu + 16], rcx         ; user RIP
    mov  [syscall_cpu + 24], r11         ; user RFLAGS
    mov  [syscall_cpu + 32], rax         ; syscall number
    mov  r9, rsp                         ; save user RSP
    mov  rcx, [syscall_cpu]              ; kernel stack top
    sub  rcx, 0xB0                       ; room for registers_t frame

    ; Build trap_frame_t on kernel stack
    mov  [rcx + 0x00], r15
    mov  [rcx + 0x08], r14
    ...
    mov  [rcx + 0x48], rdi     ; arg1
    mov  [rcx + 0x50], rsi     ; arg2
    mov  [rcx + 0x58], rdx     ; arg3
    mov  [rcx + 0x28], r10     ; arg4
    mov  [rcx + 0x38], r8      ; arg5
    mov  [rcx + 0x30], r9      ; arg6 (from stash)
    ...
    mov  rsp, rcx
    mov  rdi, rsp              ; registers_t *regs
    call syscall_dispatch

    ; Return path: restore all GPRs, sysretq
    mov  rax, [rsp + 0x70]     ; return value
    mov  rcx, [rsp + 0x88]     ; user RIP
    mov  r11, [rsp + 0x98]     ; user RFLAGS
    ...
    mov  rsp, [rsp + 0xA0]     ; user RSP
    o64 sysret
```

---

## Process Isolation (Ring 3)

### Per-Process Address Space
- Each user process has a private `page_directory_t` (cloned from kernel at `process_create_user`)
- `CR3` switched on context switch via `paging_switch_directory()` in `scheduler_schedule()`
- User VMA range: `0x40000000` – `0xBFFFF000` (below kernel heap, above identity map)

### Kernel Stack & TSS
- Each process owns a 64 KiB contiguous kernel stack (`kernel_stack_base` → `kernel_stack`)
- On schedule: `tss_set_rsp0(proc->kernel_stack)` updates TSS `RSP0` for Ring-3 → Ring-0 interrupts
- `SYSCALL` kernel stack set via `syscall_set_kernel_stack(proc->kernel_stack)`

### User-Mode Entry
First entry to Ring 3 uses `usermode_enter_trampoline`:
```asm
usermode_enter_trampoline:
    cli
    push qword 0x1B          ; SS
    push rsi                 ; user RSP
    push qword 0x202         ; RFLAGS (IF=1)
    push qword 0x23          ; CS
    push rdi                 ; user RIP (entry point)
    iretq
```

Subsequent returns from syscalls/interrupts use `SYSRET` (fast path) or `IRETQ` (interrupts).

---

## File Descriptors

Per-process `fd_table[MAX_OPEN_FILES]` (16 entries):
- FD 0, 1, 2 → `/dev/console` (stdin, stdout, stderr)
- FD 3+ → allocated via `process_alloc_fd()`
- Each entry: `vfs_node_t *node`, `offset`, `flags`, `in_use`

Shared memory segments use high-bit marker in `node` pointer (`0x80000000 | shmid`).

---

## Memory Layout (User View)

```
0x00000000_00000000 ────────────────────────
                   │  Unmapped (NULL guard)  │
0x00000000_40000000 ────────────────────────  ELF_USER_VMA_MIN
                   │  ELF text/data/bss      │  (loaded by elf_load)
                   │  ...                    │
                   │  mmap region            │  (0x40000000–0x80000000)
                   │  ...                    │
                   │  User stack (grows down)│  0xBFFFF000 - 32 KiB
0x00000000_BFFFF000 ────────────────────────  USER_STACK_TOP (guard page below)
                   │  Unmapped (kernel)      │
0xFFFF8000_00000000 ────────────────────────  Higher-half kernel alias
```

---

## Implementation Files

| File | Purpose |
|------|---------|
| `kernel/syscall64.c` | MSR setup, `SYSCALL`/`SYSRET` configuration |
| `kernel/context_switch.asm` | `syscall_entry64`, `usermode_enter_trampoline`, `fork_child_iret` |
| `kernel/syscall.c` | Syscall table, dispatch, individual implementations |
| `kernel/process.c` | Process creation, fork, exit, fd table, memory mgmt |
| `kernel/paging.c` | Page tables, CR3 switching, clone/free directories |
| `kernel/tss.c` | TSS `RSP0` management |
| `kernel/scheduler.c` | Context switch, CR3/TSS/stack updates on schedule |
| `user/libc.c` | User-space syscall wrappers (SysV ABI) |
| `user/crt0.asm` | User entry point (`_start`) |

---

## Future Extensions

Planned syscalls for later phases:
- `SYS_SIGACTION` / `SYS_SIGRETURN` / `SYS_SIGPROCMASK` — Signal handling
- `SYS_IOCTL` — Device control
- `SYS_FSTAT` / `SYS_LSTAT` — File status
- `SYS_DUP` / `SYS_DUP2` — FD duplication
- `SYS_PIPE` — Pipe creation
- `SYS_SOCKET` family — Networking
- `SYS_FUTEX` — Userspace synchronization