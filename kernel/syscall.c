#include "syscall.h"
#include "process.h"
#include "heap.h"
#include "timer.h"
#include "idt.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../fs/vfs.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern char keyboard_getchar(void);

/* User pointer validation. With the kernel identity-mapped below 0x40000000
 * (kernel image, heap, PMM structures, MMIO) and a higher-half alias at
 * 0xFFFF800000000000, only the band between the two can be legitimate user
 * space (mmap region 0x40000000+, user stack near 0xBFFFF…). This replaces
 * the previous "src < 0xC0000000" test which wrongly admitted the heap.
 * NOTE: fault-tolerant copy-through on unmapped pages is Phase 3 (SMAP +
 * probing); this is the range gate only. */
#define USER_ADDR_MIN   0x40000000ULL
#define USER_ADDR_MAX   0xFFFF800000000000ULL

static int in_user_range(uintptr_t addr, size_t n) {
    if (n > 0 && (addr < USER_ADDR_MIN || addr >= USER_ADDR_MAX)) return 0;
    if (n == 0) return 1;
    uintptr_t end = addr + n;
    return end > addr && end <= USER_ADDR_MAX;
}

static int copy_from_user(void *dst, const void *src, size_t n) {
    if (!dst || !src) return -1;
    if (!in_user_range((uintptr_t)src, n)) return -1;
    memcpy(dst, src, n);
    return 0;
}
static __attribute__((unused)) int copy_to_user(void *dst, const void *src, size_t n) {
    if (!dst || !src) return -1;
    if (!in_user_range((uintptr_t)dst, n)) return -1;
    memcpy(dst, src, n);
    return 0;
}

typedef int32_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static syscall_fn syscall_table[NUM_SYSCALLS] = {0};
static registers_t *g_current_regs;

/* --- Individual syscall implementations --- */

static int32_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    process_exit((int)code);
    return 0;   /* Never reached */
}

static int32_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    /* process_fork needs the caller's full syscall frame (to build the
     * child's resume state); it is captured by syscall_dispatch. */
    return process_fork(g_current_regs);
}

static int32_t sys_read(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                        uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    process_t *proc = process_get_current();
    if (fd >= MAX_OPEN_FILES || !proc->fd_table[fd].in_use)
        return -1;

    vfs_node_t *node = proc->fd_table[fd].node;
    if (!node) return -1;

    /* Special case: reading from console/keyboard */
    if (node == vfs_resolve_path("/dev/console")) {
        if (!in_user_range(buf_ptr, count)) return -1;
        char *buf = (char *)(uintptr_t)buf_ptr;
        for (uint32_t i = 0; i < count; i++) {
            buf[i] = keyboard_getchar();
            if (buf[i] == '\n') {
                return i + 1;
            }
        }
        return count;
    }

    if (!in_user_range(buf_ptr, count)) return -1;
    int bytes = vfs_read(node, proc->fd_table[fd].offset, count, (void *)(uintptr_t)buf_ptr);
    if (bytes > 0)
        proc->fd_table[fd].offset += bytes;
    return bytes;
}

static int32_t sys_write(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                         uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    process_t *proc = process_get_current();
    if (fd >= MAX_OPEN_FILES || !proc->fd_table[fd].in_use)
        return -1;

    if (!in_user_range(buf_ptr, count)) return -1;
    const char *buf = (const char *)(uintptr_t)buf_ptr;

    /* Special case: writing to console */
    vfs_node_t *console = vfs_resolve_path("/dev/console");
    if (proc->fd_table[fd].node == console || fd == 1 || fd == 2) {
        for (uint32_t i = 0; i < count; i++)
            screen_putchar(buf[i]);
        return count;
    }

    vfs_node_t *node = proc->fd_table[fd].node;
    if (!node) return -1;

    int bytes = vfs_write(node, proc->fd_table[fd].offset, count, buf);
    if (bytes > 0)
        proc->fd_table[fd].offset += bytes;
    return bytes;
}

static int32_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    process_t *proc = process_get_current();
    char path_buf[256];
    if (copy_from_user(path_buf, (const void *)(uintptr_t)path_ptr, 255) != 0) return -1;
    path_buf[255] = '\0';
    vfs_node_t *node = vfs_resolve_path(path_buf);
    if (!node) return -1;

    int fd = process_alloc_fd(proc);
    if (fd < 0) return -1;

    proc->fd_table[fd].node = node;
    proc->fd_table[fd].offset = 0;
    proc->fd_table[fd].flags = flags;
    return fd;
}

static int32_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    process_t *proc = process_get_current();
    process_free_fd(proc, (int)fd);
    return 0;
}

static int32_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return process_get_current()->pid;
}

static int32_t sys_wait(uint64_t pid, uint64_t status_ptr, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    int status = 0;
    int ret = process_wait((pid_t)pid, status_ptr ? &status : NULL);
    if (ret > 0 && status_ptr && in_user_range(status_ptr, sizeof(int)))
        *(int *)(uintptr_t)status_ptr = status;
    return ret;
}

static int32_t sys_kill(uint64_t pid, uint64_t sig, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    return process_kill((pid_t)pid, (int)sig);
}

static int32_t sys_sleep(uint64_t ms, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    process_sleep(ms);
    return 0;
}

static int32_t sys_yield(uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_yield();
    return 0;
}

static int32_t sys_time(uint64_t a1, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (int32_t)timer_get_seconds();
}

static int32_t sys_putchar(uint64_t c, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    screen_putchar((char)c);
    return 0;
}

static int32_t sys_getchar(uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return keyboard_getchar();
}

static int32_t sys_ps(uint64_t a1, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    process_dump_all();
    return 0;
}

static int32_t sys_uptime(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (int32_t)timer_get_seconds();
}

static int32_t sys_execve(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    char path_buf[256];
    if (copy_from_user(path_buf, (const void *)(uintptr_t)a1, 255) != 0) return -1;
    path_buf[255] = '\0';
    const char *path = path_buf;
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return -1;
    uint32_t size = node->length;
    if (size == 0) return -1;
    uint8_t *data = (uint8_t *)kmalloc(size);
    if (!data) return -1;
    int bytes = vfs_read(node, 0, size, data);
    if (bytes < 0 || (uint32_t)bytes != size) {
        kfree(data);
        return -1;
    }
    char name[PROCESS_NAME_LEN];
    const char *base = path;
    const char *p = path;
    while (*p) {
        if (*p == '/') base = p + 1;
        p++;
    }
    strncpy(name, base, PROCESS_NAME_LEN - 1);
    name[PROCESS_NAME_LEN - 1] = '\0';
    process_t *proc = process_create_user(name, data, size);
    kfree(data);
    if (!proc) return -1;
    return proc->pid;
}

void syscall_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));

    syscall_table[SYS_EXIT]    = sys_exit;
    syscall_table[SYS_FORK]    = sys_fork;
    syscall_table[SYS_READ]    = sys_read;
    syscall_table[SYS_WRITE]   = sys_write;
    syscall_table[SYS_OPEN]    = sys_open;
    syscall_table[SYS_CLOSE]   = sys_close;
    syscall_table[SYS_GETPID]  = sys_getpid;
    syscall_table[SYS_WAIT]    = sys_wait;
    syscall_table[SYS_KILL]    = sys_kill;
    syscall_table[SYS_SLEEP]   = sys_sleep;
    syscall_table[SYS_YIELD]   = sys_yield;
    syscall_table[SYS_TIME]    = sys_time;
    syscall_table[SYS_PUTCHAR] = sys_putchar;
    syscall_table[SYS_GETCHAR] = sys_getchar;
    syscall_table[SYS_PS]      = sys_ps;
    syscall_table[SYS_UPTIME]  = sys_uptime;
    syscall_table[SYS_EXECVE]  = sys_execve;

    kprintf("[SYSCALL] System call interface initialized (%d calls)\n",
            NUM_SYSCALLS);
}

/* Called from syscall_entry in ASM.  Args follow the SysV convention:
 * rdi, rsi, rdx, r10, r8.  Errors are returned as negative errno values. */
void syscall_dispatch(registers_t *regs) {
    uint32_t syscall_num = regs->rax;
    g_current_regs = regs;

    if (syscall_num >= NUM_SYSCALLS || !syscall_table[syscall_num]) {
        kprintf("[SYSCALL] Invalid syscall number: %d\n", syscall_num);
        regs->rax = (uint64_t)-ENOSYS;
        return;
    }

    regs->rax = (uint64_t)syscall_table[syscall_num](
        regs->rdi,    /* arg1 */
        regs->rsi,    /* arg2 */
        regs->rdx,    /* arg3 */
        regs->r10,    /* arg4 */
        regs->r8      /* arg5 */
    );
}
