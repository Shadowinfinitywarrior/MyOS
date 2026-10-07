#include "syscall.h"
#include "process.h"
#include "heap.h"
#include "timer.h"
#include "idt.h"
#include "mmap.h"
#include "shm.h"
#include "ipc.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../fs/vfs.h"
#include "vtty.h"
#include "../include/rust_gui.h"

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
    if (!dst) return -1;
    if (!src && n) return -1;
    if (!in_user_range((uintptr_t)src, n)) return -1;
    if (n) {
        stac();
        memcpy(dst, src, n);
        clac();
    }
    return 0;
}
static int copy_to_user(void *dst, const void *src, size_t n) {
    if (!dst) return -1;
    if (!src && n) return -1;
    if (!in_user_range((uintptr_t)dst, n)) return -1;
    if (n) {
        stac();
        memcpy(dst, src, n);
        clac();
    }
    return 0;
}

/* Copy a NUL-terminated user string into a kernel buffer, stopping at the
 * terminator. Reads byte-at-a-time under stac so a string that ends near a
 * page boundary cannot fault the kernel by over-reading past the mapping. */
static int copy_str_from_user(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return -1;
    if ((uintptr_t)src < USER_ADDR_MIN) return -1;
    size_t i = 0;
    stac();
    for (; i < dst_size - 1; i++) {
        char c = src[i];
        dst[i] = c;
        if (c == '\0') { clac(); return 0; }
    }
    clac();
    dst[dst_size - 1] = '\0';
    return -1;  /* no NUL within capacity */
}

typedef int32_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static syscall_fn syscall_table[NUM_SYSCALLS] = {0};
registers_t *g_current_regs;

/* --- Individual syscall implementations --- */

static int32_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6;
    process_exit((int)code);
    return 0;   /* Never reached */
}

static int32_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    /* process_fork needs the caller's full syscall frame (to build the
     * child's resume state); it is captured by syscall_dispatch. */
    return process_fork(g_current_regs);
}

static int32_t sys_read(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (fd >= MAX_OPEN_FILES || !proc->fd_table[fd].in_use)
        return -1;

    vfs_node_t *node = proc->fd_table[fd].node;
    if (!node) return -1;
    if (count == 0) return 0;
    if (!in_user_range(buf_ptr, count)) return -1;

    uint8_t bounce[256];
    uint32_t total = 0;

    /* Console read: pull from the process's virtual terminal. In a graphical
     * session the GUI feeds that terminal from the focused window's keyboard,
     * so the same code serves the serial console and a window with no syscall
     * differences. */
    if (node == vfs_resolve_path("/dev/console")) {
        vtty_t *v = proc->vtty ? proc->vtty : console_boot();
        while (total < count) {
            uint32_t n = (uint32_t)(count - total);
            if (n > sizeof(bounce)) n = sizeof(bounce);
            uint32_t got = 0;
            while (got < n) {
                char c;
                if (!v || !vtty_pop_char(v, &c)) {
                    /* No input yet: let other processes run rather than
                     * spinning the scheduler in a tight loop. */
                    process_yield();
                    continue;
                }
                /* Echo so the typed line appears in the terminal. */
                vtty_putc(v, c);
                bounce[got++] = (uint8_t)c;
                if (c == '\n') break;
            }
            if (copy_to_user((void *)(uintptr_t)(buf_ptr + total), bounce, got) != 0)
                return -1;
            total += got;
            if (got < n) break;
        }
        return (int32_t)total;
    }

    while (total < count) {
        uint32_t n = (uint32_t)(count - total);
        if (n > sizeof(bounce)) n = sizeof(bounce);
        int bytes = vfs_read(node, proc->fd_table[fd].offset + total, n, bounce);
        if (bytes < 0) return bytes;
        if (bytes == 0) break;
        if (copy_to_user((void *)(uintptr_t)(buf_ptr + total), bounce,
                         (size_t)bytes) != 0)
            return -1;
        total += (uint32_t)bytes;
        if ((uint32_t)bytes < n) break;
    }
    proc->fd_table[fd].offset += total;
    return (int32_t)total;
}

static int32_t sys_write(uint64_t fd, uint64_t buf_ptr, uint64_t count,
                         uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (fd >= MAX_OPEN_FILES || !proc->fd_table[fd].in_use)
        return -1;

    if (count == 0) return 0;
    if (!in_user_range(buf_ptr, count)) return -1;

    vfs_node_t *node = proc->fd_table[fd].node;
    int to_console = (node == vfs_resolve_path("/dev/console")) || fd == 1 || fd == 2;
    if (!to_console && !node) return -1;

    /* Console output goes to the process's virtual terminal. The boot terminal
     * is mirrored to serial by the console writer, so a headless boot still
     * sees kernel log output while a windowed shell does not pollute it. */
    vtty_t *cv = proc->vtty ? proc->vtty : console_boot();

    uint8_t bounce[256];
    uint32_t total = 0;
    while (total < count) {
        uint32_t n = (uint32_t)(count - total);
        if (n > sizeof(bounce)) n = sizeof(bounce);
        if (copy_from_user(bounce, (const void *)(uintptr_t)(buf_ptr + total), n) != 0)
            return -1;
        if (to_console) {
            if (cv) {
                vtty_write(cv, (const char *)bounce, (int)n);
            } else {
                for (uint32_t i = 0; i < n; i++) screen_putchar(bounce[i]);
            }
            total += n;
            continue;
        }
        int bytes = vfs_write(node, proc->fd_table[fd].offset + total, n, bounce);
        if (bytes < 0) return bytes;
        if (bytes == 0) break;
        proc->fd_table[fd].offset += bytes;
        total += (uint32_t)bytes;
        if ((uint32_t)bytes < n) break;
    }
    return (int32_t)total;
}

static int32_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a6;
    process_t *proc = process_get_current();
    char path_buf[256];
    if (copy_str_from_user(path_buf, sizeof(path_buf),
                           (const char *)(uintptr_t)path_ptr) != 0) return -1;
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
                         uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6;
    process_t *proc = process_get_current();
    process_free_fd(proc, (int)fd);
    return 0;
}

static int32_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    return process_get_current()->pid;
}

static int32_t sys_wait(uint64_t pid, uint64_t status_ptr, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a6;
    int status = 0;
    int ret = process_wait((pid_t)pid, status_ptr ? &status : NULL);
    if (ret > 0 && status_ptr) {
        if (copy_to_user((void *)(uintptr_t)status_ptr, &status, sizeof(status)) != 0)
            return -1;
    }
    return ret;
}

static int32_t sys_kill(uint64_t pid, uint64_t sig, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a6;
    return process_kill((pid_t)pid, (int)sig);
}

static int32_t sys_sleep(uint64_t ms, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6;
    process_sleep(ms);
    return 0;
}

static int32_t sys_yield(uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    process_yield();
    return 0;
}

static int32_t sys_time(uint64_t a1, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    return (int32_t)timer_get_seconds();
}

static int32_t sys_putchar(uint64_t c, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6;
    /* Must go to the same destination as sys_write. libc puts() emits the
     * string with write() and the trailing newline with putchar(), so sending
     * putchar() to the legacy VGA text console instead of the process's
     * virtual terminal splits one line across two devices: the text shows up
     * in the terminal window but its newlines vanish, so every line is
     * appended to the end of the previous one and multi-line output renders
     * as one long wrapped run. */
    process_t *proc = process_get_current();
    vtty_t *cv = proc ? (proc->vtty ? proc->vtty : console_boot()) : NULL;
    if (cv) vtty_putc(cv, (char)c);
    else    screen_putchar((char)c);
    return 0;
}

static int32_t sys_getchar(uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    char c = 0;
    while ((c = keyboard_getchar()) == 0) {
        process_yield();
    }
    return c;
}

static int32_t sys_ps(uint64_t a1, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    process_dump_all();
    return 0;
}

static int32_t sys_uptime(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6; (void)a6;
    return (int32_t)timer_get_seconds();
}

static int32_t sys_execve(uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a6; (void)a6;
    char path_buf[256];
    if (copy_str_from_user(path_buf, sizeof(path_buf),
                           (const char *)(uintptr_t)a1) != 0) return -1;
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

/* --- mmap/munmap/mprotect Syscalls --- */

static int32_t syscall_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                            uint64_t flags, uint64_t fd, uint64_t offset) {
    process_t *proc = process_get_current();
    if (!proc) return (int32_t)(uintptr_t)MAP_FAILED;
    void *ret = sys_mmap(proc, (uint32_t)addr, (uint32_t)length,
                         (uint32_t)prot, (uint32_t)flags, (int)fd, (uint32_t)offset);
    return (int32_t)(uintptr_t)ret;
}

static int32_t syscall_munmap(uint64_t addr, uint64_t length, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;
    return sys_munmap(proc, (uint32_t)addr, (uint32_t)length);
}

static int32_t syscall_mprotect(uint64_t addr, uint64_t length, uint64_t prot,
                                uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;
    return sys_mprotect(proc, (uint32_t)addr, (uint32_t)length, (uint32_t)prot);
}

/* --- Shared Memory Syscalls --- */

extern int shm_create(const char *name, uint32_t size);
extern int shm_open(const char *name);
extern void *shm_attach(int shmid);
extern int shm_detach(int shmid, void *addr);
extern int shm_destroy(int shmid);

static int32_t sys_shmget(uint64_t name_ptr, uint64_t size, uint64_t flags,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)flags; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    char name[SHM_NAME_MAX];
    if (copy_str_from_user(name, sizeof(name), (const char *)(uintptr_t)name_ptr) != 0)
        return -1;
    
    /* Create or open shared memory segment */
    int shmid = shm_create(name, (uint32_t)size);
    if (shmid < 0) {
        /* Try to open existing */
        shmid = shm_open(name);
        if (shmid < 0) return -1;
    }
    
    /* Return a special fd for shm (using high bit to mark as shm) */
    int fd = process_alloc_fd(proc);
    if (fd < 0) return -1;
    
    /* Store shmid in fd table (using node pointer as shmid marker) */
    proc->fd_table[fd].in_use = 1;
    proc->fd_table[fd].node = (vfs_node_t *)(uintptr_t)(shmid | 0x80000000);
    proc->fd_table[fd].offset = 0;
    proc->fd_table[fd].flags = 0;
    
    return fd;
}

static int32_t sys_shmctl(uint64_t fd, uint64_t cmd, uint64_t arg,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;
    if (fd >= MAX_OPEN_FILES || !proc->fd_table[fd].in_use) return -1;
    
    /* Check if it's an shm fd */
    uintptr_t node_ptr = (uintptr_t)proc->fd_table[fd].node;
    if (!(node_ptr & 0x80000000)) return -1;
    int shmid = node_ptr & 0x7FFFFFFF;
    
    switch ((int)cmd) {
        case 0: /* SHM_ATTACH - map into address space via mmap */
            return shmid;
        case 1: /* SHM_DETACH */
            return shm_detach(shmid, (void *)(uintptr_t)arg);
        case 2: /* SHM_DESTROY */
            process_free_fd(proc, (int)fd);
            return shm_destroy(shmid);
        default:
            return -1;
    }
}

/* --- GUI Syscalls (Phase 3: Go Shell -> Rust GUI) --- */

extern void *rust_gui_create_surface(int width, int height);
extern int rust_gui_blit_surface(void *surface, int x, int y);
extern void rust_gui_invalidate_window(uint64_t id, int32_t x, int32_t y, int32_t w, int32_t h);
extern void rust_gui_get_fb_info(int *width, int *height, int *pitch);

/* --- MYDP Syscalls (Phase 5) --- */
extern void rust_gui_destroy_window(uint64_t id);

static int32_t sys_gui_create_surface(uint64_t w, uint64_t h, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    void *surf = rust_gui_create_surface((int)w, (int)h);
    return (int32_t)(uintptr_t)surf;
}

static int32_t sys_gui_blit_surface(uint64_t surf, uint64_t x, uint64_t y,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    return rust_gui_blit_surface((void *)(uintptr_t)surf, (int)x, (int)y);
}

static int32_t sys_gui_invalidate(uint64_t id, uint64_t x, uint64_t y,
                                  uint64_t w, uint64_t h, uint64_t a6) {
    (void)a6;
    rust_gui_invalidate_window(id, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h);
    return 0;
}

static int32_t sys_gui_get_fb_info(uint64_t w_ptr, uint64_t h_ptr, uint64_t p_ptr,
                                   uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    int width, height, pitch;
    rust_gui_get_fb_info(&width, &height, &pitch);

    if (copy_to_user((void *)(uintptr_t)w_ptr, &width, sizeof(width)) != 0) return -1;
    if (copy_to_user((void *)(uintptr_t)h_ptr, &height, sizeof(height)) != 0) return -1;
    if (copy_to_user((void *)(uintptr_t)p_ptr, &pitch, sizeof(pitch)) != 0) return -1;
    return 0;
}

/* --- Extended GUI Syscalls for Java/GraalVM (Phase 4) --- */

static int32_t sys_gui_init(uint64_t width, uint64_t height, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    return rust_gui_init((uint32_t)width, (uint32_t)height);
}

static int32_t sys_gui_create_window(uint64_t title_ptr, uint64_t x, uint64_t y,
                                     uint64_t w, uint64_t h, uint64_t a6) {
    (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return 0;

    char title_buf[64];
    if (copy_str_from_user(title_buf, sizeof(title_buf),
                           (const char *)(uintptr_t)title_ptr) != 0) {
        return 0;
    }

    window_id_t id = rust_gui_create_window(title_buf, (int32_t)x, (int32_t)y,
                                            (int32_t)w, (int32_t)h);
    return (int32_t)id;
}

static int32_t sys_gui_destroy_window(uint64_t id, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    rust_gui_destroy_window((window_id_t)id);
    return 0;
}

static int32_t sys_gui_render_frame(uint64_t a1, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return rust_gui_render_frame();
}

static int32_t sys_gui_set_framebuffer(uint64_t fb, uint64_t width, uint64_t height,
                                       uint64_t pitch, uint64_t a5, uint64_t a6) {
    (void)a5; (void)a6;
    rust_gui_set_framebuffer((uint32_t *)(uintptr_t)fb, (uint32_t)width,
                             (uint32_t)height, (uint32_t)pitch);
    return 0;
}

static int32_t sys_gui_window_count(uint64_t a1, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return rust_gui_window_count();
}

static int32_t sys_gui_get_window(uint64_t index, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    window_t *win = rust_gui_get_window((int32_t)index);
    return (int32_t)(uintptr_t)win;
}

static int32_t sys_gui_push_key_event(uint64_t event_type, uint64_t scancode,
                                      uint64_t ascii, uint64_t modifiers,
                                      uint64_t a5, uint64_t a6) {
    (void)a5; (void)a6;
    rust_gui_push_key_event((uint8_t)event_type, (uint8_t)scancode,
                            (uint8_t)ascii, (uint32_t)modifiers);
    return 0;
}

static int32_t sys_gui_push_mouse_event(uint64_t event_type, uint64_t x,
                                        uint64_t y, uint64_t button,
                                        uint64_t delta, uint64_t a6) {
    (void)a6;
    rust_gui_push_mouse_event((uint8_t)event_type, (int32_t)x, (int32_t)y,
                              (uint8_t)button, (int32_t)delta);
    return 0;
}

static int32_t sys_gui_focus_window(uint64_t id, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return rust_gui_focus_window((window_id_t)id);
}

static int32_t sys_gui_set_window_title(uint64_t id, uint64_t title_ptr,
                                        uint64_t a3, uint64_t a4,
                                        uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    char title_buf[64];
    if (copy_str_from_user(title_buf, sizeof(title_buf),
                           (const char *)(uintptr_t)title_ptr) != 0) {
        return -1;
    }

    return rust_gui_set_window_title((window_id_t)id, title_buf);
}

static int32_t sys_gui_get_window_rect(uint64_t id, uint64_t rect_ptr,
                                       uint64_t a3, uint64_t a4,
                                       uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    rect_t rect;
    int result = rust_gui_get_window_rect((window_id_t)id, &rect);
    if (result <= 0) return result;

    if (copy_to_user((void *)(uintptr_t)rect_ptr, &rect, sizeof(rect)) != 0)
        return -1;
    return 1;
}

static int32_t sys_gui_set_window_rect(uint64_t id, uint64_t rect_ptr,
                                       uint64_t a3, uint64_t a4,
                                       uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    rect_t rect;
    if (copy_from_user(&rect, (const void *)(uintptr_t)rect_ptr, sizeof(rect)) != 0)
        return -1;

    return rust_gui_set_window_rect((window_id_t)id, &rect);
}

/* --- IPC/MYDP Syscalls for Cross-Language Integration --- */
/* All IPC functions are declared in ipc.h, no need to redeclare */

static int32_t sys_ipc_port_create(uint64_t name_ptr, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    char name[64];
    if (copy_str_from_user(name, sizeof(name), (const char *)(uintptr_t)name_ptr) != 0)
        return -1;

    return ipc_port_create(name);
}

static int32_t sys_ipc_port_destroy(uint64_t port_id, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return ipc_port_destroy((int)port_id);
}

static int32_t sys_ipc_port_send(uint64_t port_id, uint64_t msg_ptr, uint64_t a3,
                                 uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    /* Copy message from user space */
    ipc_msg_t msg;
    if (copy_from_user(&msg, (const void *)(uintptr_t)msg_ptr, sizeof(ipc_msg_t)) != 0)
        return -1;

    return ipc_port_send((int)port_id, &msg);
}

static int32_t sys_ipc_port_recv(uint64_t port_id, uint64_t msg_ptr, uint64_t block,
                                 uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    ipc_msg_t msg;
    int ret = ipc_port_recv((int)port_id, &msg, (bool)block);
    if (ret < 0) return ret;

    if (copy_to_user((void *)(uintptr_t)msg_ptr, &msg, sizeof(ipc_msg_t)) != 0)
        return -1;
    return 0;
}

static int32_t sys_ipc_port_find(uint64_t name_ptr, uint64_t a2, uint64_t a3,
                                 uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    char name[64];
    if (copy_str_from_user(name, sizeof(name), (const char *)(uintptr_t)name_ptr) != 0)
        return -1;

    return ipc_port_find(name);
}

static int32_t sys_ipc_cap_grant(uint64_t target_pid, uint64_t port_id, uint64_t rights,
                                 uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    return ipc_cap_grant((pid_t)target_pid, (uint32_t)port_id, (uint32_t)rights);
}

static int32_t sys_ipc_cap_revoke(uint64_t target_pid, uint64_t port_id, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    return ipc_cap_revoke((pid_t)target_pid, (uint32_t)port_id);
}

static int32_t sys_ipc_shm_create(uint64_t name_ptr, uint64_t size, uint64_t shmid_ptr,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    char name[64];
    if (copy_str_from_user(name, sizeof(name), (const char *)(uintptr_t)name_ptr) != 0)
        return -1;

    uint32_t shmid;
    int ret = ipc_shm_create(name, (uint32_t)size, &shmid);
    if (ret >= 0) {
        if (copy_to_user((void *)(uintptr_t)shmid_ptr, &shmid, sizeof(shmid)) != 0)
            return -1;
    }
    return ret;
}

static int32_t sys_ipc_shm_attach(uint64_t shmid, uint64_t addr_ptr, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    void *addr;
    int ret = ipc_shm_attach((int)shmid, &addr);
    if (ret >= 0) {
        if (copy_to_user((void *)(uintptr_t)addr_ptr, &addr, sizeof(addr)) != 0)
            return -1;
    }
    return ret;
}

static int32_t sys_ipc_shm_detach(uint64_t shmid, uint64_t addr, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    return ipc_shm_detach((int)shmid, (void *)(uintptr_t)addr);
}

static int32_t sys_ipc_shm_destroy(uint64_t shmid, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return ipc_shm_destroy((int)shmid);
}

static int32_t sys_ipc_event_subscribe(uint64_t event_type, uint64_t handler_ptr,
                                       uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)event_type; (void)handler_ptr; (void)a3; (void)a4; (void)a5; (void)a6;
    /* Event subscription from userspace not directly supported for security */
    return -ENOSYS;
}

static int32_t sys_ipc_event_publish(uint64_t event_type, uint64_t msg_ptr,
                                     uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    ipc_msg_t msg;
    if (copy_from_user(&msg, (const void *)(uintptr_t)msg_ptr, sizeof(ipc_msg_t)) != 0)
        return -1;

    return ipc_event_publish((uint32_t)event_type, &msg);
}

/* --- MYDP Protocol Syscalls --- */

static int32_t sys_mydp_create_surface(uint64_t width, uint64_t height, uint64_t surface_ptr,
                                       uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    extern void *rust_gui_create_surface(int width, int height);
    void *surface = rust_gui_create_surface((int)width, (int)height);
    
    if (copy_to_user((void *)(uintptr_t)surface_ptr, &surface, sizeof(surface)) != 0)
        return -1;
    return surface ? 0 : -1;
}

static int32_t sys_mydp_attach_buffer(uint64_t surface_id, uint64_t buffer_id, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)surface_id; (void)buffer_id; (void)a3; (void)a4; (void)a5; (void)a6;
    /* Map shared memory buffer to surface */
    return 0;  /* Not fully implemented yet */
}

static int32_t sys_mydp_commit(uint64_t surface_id, uint64_t a2, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    /* Commit surface damage - invalidate full window */
    rust_gui_invalidate_window(surface_id, 0, 0, 0, 0);
    return 0;
}

static int32_t sys_mydp_damage(uint64_t surface_id, uint64_t x, uint64_t y,
                               uint64_t w, uint64_t h, uint64_t a6) {
    (void)a6;
    rust_gui_invalidate_window(surface_id, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h);
    return 0;
}

static int32_t sys_mydp_close_surface(uint64_t surface_id, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    rust_gui_destroy_window(surface_id);
    return 0;
}

static int32_t sys_mydp_get_fb_info(uint64_t width_ptr, uint64_t height_ptr, uint64_t pitch_ptr,
                                    uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    process_t *proc = process_get_current();
    if (!proc) return -1;

    int width, height, pitch;
    extern void rust_gui_get_fb_info(int *width, int *height, int *pitch);
    rust_gui_get_fb_info(&width, &height, &pitch);

    if (copy_to_user((void *)(uintptr_t)width_ptr, &width, sizeof(width)) != 0) return -1;
    if (copy_to_user((void *)(uintptr_t)height_ptr, &height, sizeof(height)) != 0) return -1;
    if (copy_to_user((void *)(uintptr_t)pitch_ptr, &pitch, sizeof(pitch)) != 0) return -1;
    return 0;
}

/* --- Signal Handling Syscalls --- */

extern int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
extern int sys_sigprocmask(int how, const uint32_t *set, uint32_t *oldset);
extern int sys_sigreturn(registers_t *saved_regs);

static int32_t sys_sigaction_wrapper(uint64_t signum, uint64_t act_ptr, uint64_t oldact_ptr,
                                     uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    return sys_sigaction((int)signum, (const struct sigaction *)(uintptr_t)act_ptr,
                         (struct sigaction *)(uintptr_t)oldact_ptr);
}

static int32_t sys_sigprocmask_wrapper(uint64_t how, uint64_t set_ptr, uint64_t oldset_ptr,
                                       uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    return sys_sigprocmask((int)how, (const uint32_t *)(uintptr_t)set_ptr,
                           (uint32_t *)(uintptr_t)oldset_ptr);
}

static int32_t sys_sigreturn_wrapper(uint64_t saved_regs_ptr, uint64_t a2, uint64_t a3,
                                     uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return sys_sigreturn((registers_t *)(uintptr_t)saved_regs_ptr);
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
    syscall_table[SYS_MMAP]    = syscall_mmap;
    syscall_table[SYS_MUNMAP]  = syscall_munmap;
    syscall_table[SYS_MPROTECT] = syscall_mprotect;
    syscall_table[SYS_SHMGET]  = sys_shmget;
    syscall_table[SYS_SHMCTL]  = sys_shmctl;
    syscall_table[SYS_GUI_CREATE_SURFACE] = sys_gui_create_surface;
    syscall_table[SYS_GUI_BLIT_SURFACE]   = sys_gui_blit_surface;
    syscall_table[SYS_GUI_INVALIDATE]     = sys_gui_invalidate;
    syscall_table[SYS_GUI_GET_FB_INFO]    = sys_gui_get_fb_info;
    syscall_table[SYS_GUI_INIT]                  = sys_gui_init;
    syscall_table[SYS_GUI_CREATE_WINDOW]         = sys_gui_create_window;
    syscall_table[SYS_GUI_DESTROY_WINDOW]        = sys_gui_destroy_window;
    syscall_table[SYS_GUI_RENDER_FRAME]          = sys_gui_render_frame;
    syscall_table[SYS_GUI_SET_FRAMEBUFFER]       = sys_gui_set_framebuffer;
    syscall_table[SYS_GUI_WINDOW_COUNT]          = sys_gui_window_count;
    syscall_table[SYS_GUI_GET_WINDOW]            = sys_gui_get_window;
    syscall_table[SYS_GUI_PUSH_KEY_EVENT]        = sys_gui_push_key_event;
    syscall_table[SYS_GUI_PUSH_MOUSE_EVENT]      = sys_gui_push_mouse_event;
    syscall_table[SYS_GUI_FOCUS_WINDOW]          = sys_gui_focus_window;
    syscall_table[SYS_GUI_SET_WINDOW_TITLE]      = sys_gui_set_window_title;
    syscall_table[SYS_GUI_GET_WINDOW_RECT]       = sys_gui_get_window_rect;
    syscall_table[SYS_GUI_SET_WINDOW_RECT]       = sys_gui_set_window_rect;
    syscall_table[SYS_IPC_PORT_CREATE]       = sys_ipc_port_create;
    syscall_table[SYS_IPC_PORT_DESTROY]      = sys_ipc_port_destroy;
    syscall_table[SYS_IPC_PORT_SEND]         = sys_ipc_port_send;
    syscall_table[SYS_IPC_PORT_RECV]         = sys_ipc_port_recv;
    syscall_table[SYS_IPC_PORT_FIND]         = sys_ipc_port_find;
    syscall_table[SYS_IPC_CAP_GRANT]         = sys_ipc_cap_grant;
    syscall_table[SYS_IPC_CAP_REVOKE]        = sys_ipc_cap_revoke;
    syscall_table[SYS_IPC_SHM_CREATE]        = sys_ipc_shm_create;
    syscall_table[SYS_IPC_SHM_ATTACH]        = sys_ipc_shm_attach;
    syscall_table[SYS_IPC_SHM_DETACH]        = sys_ipc_shm_detach;
    syscall_table[SYS_IPC_SHM_DESTROY]       = sys_ipc_shm_destroy;
    syscall_table[SYS_IPC_EVENT_SUBSCRIBE]   = sys_ipc_event_subscribe;
    syscall_table[SYS_IPC_EVENT_PUBLISH]     = sys_ipc_event_publish;
    syscall_table[SYS_MYDP_CREATE_SURFACE]   = sys_mydp_create_surface;
    syscall_table[SYS_MYDP_ATTACH_BUFFER]    = sys_mydp_attach_buffer;
    syscall_table[SYS_MYDP_COMMIT]           = sys_mydp_commit;
    syscall_table[SYS_MYDP_DAMAGE]           = sys_mydp_damage;
    syscall_table[SYS_MYDP_CLOSE_SURFACE]    = sys_mydp_close_surface;
    syscall_table[SYS_MYDP_GET_FB_INFO]      = sys_mydp_get_fb_info;
    syscall_table[SYS_SIGACTION]      = sys_sigaction_wrapper;
    syscall_table[SYS_SIGRETURN]      = sys_sigreturn_wrapper;
    syscall_table[SYS_SIGPROCMASK]    = sys_sigprocmask_wrapper;

    kprintf("[SYSCALL] System call interface initialized (%d calls)\n",
            NUM_SYSCALLS);
}

/* Called from syscall_entry in ASM.  Args follow the SysV convention:
 * rdi, rsi, rdx, r10, r8, r9.  Errors are returned as negative errno values. */
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
        regs->r8,     /* arg5 */
        regs->r9      /* arg6 */
    );
}
