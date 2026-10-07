/*
 * MyOS Python Runtime - Minimal syscall interface
 * Provides Python C API shim for kernel syscalls
 * This is a stub implementation for build verification
 */

#include "../../../include/types.h"
#include "../../../include/system.h"
#include "../../../user/libc.h"
#include "pymyos.h"

// Module state
static bool pymyos_initialized = false;

// ============================================================================
// Syscall wrapper functions - direct syscall interface
// ============================================================================

long pymyos_syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    return _syscall(num, a1, a2, a3, a4, a5, a6);
}

void pymyos_exit(int code) {
    exit(code);
}

pid_t pymyos_fork(void) {
    return fork();
}

pid_t pymyos_exec(const char *path) {
    return exec(path);
}

int pymyos_read(int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}

int pymyos_write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

int pymyos_open(const char *path, int flags) {
    return open(path, flags);
}

int pymyos_close(int fd) {
    return close(fd);
}

pid_t pymyos_getpid(void) {
    return getpid();
}

void pymyos_sleep(unsigned int ms) {
    sleep_ms(ms);
}

void pymyos_yield(void) {
    yield();
}

int pymyos_time(void) {
    return _syscall(SYS_TIME, 0, 0, 0, 0, 0, 0);
}

int pymyos_uptime(void) {
    return uptime();
}

void pymyos_putchar(char c) {
    putchar(c);
}

char pymyos_getchar(void) {
    return getchar();
}

void *pymyos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return mmap(addr, length, prot, flags, fd, offset);
}

int pymyos_munmap(void *addr, size_t length) {
    return munmap(addr, length);
}

int pymyos_shmget(const char *name, size_t size, int flags) {
    return shmget(name, size, flags);
}

int pymyos_shmctl(int fd, int cmd, void *arg) {
    return shmctl(fd, cmd, arg);
}

// ============================================================================
// GUI syscalls
// ============================================================================

void *pymyos_gui_create_surface(int width, int height) {
    return (void *)_syscall(SYS_GUI_CREATE_SURFACE, width, height, 0, 0, 0, 0);
}

int pymyos_gui_blit_surface(void *surface, int x, int y) {
    return _syscall(SYS_GUI_BLIT_SURFACE, (long)surface, x, y, 0, 0, 0);
}

int pymyos_gui_invalidate(uint64_t id, int x, int y, int w, int h) {
    return _syscall(SYS_GUI_INVALIDATE, id, x, y, w, h, 0);
}

void pymyos_gui_get_fb_info(int *width, int *height, int *pitch) {
    _syscall(SYS_GUI_GET_FB_INFO, (long)width, (long)height, (long)pitch, 0, 0, 0);
}

// ============================================================================
// GUI drawing functions (for Python Qt bindings)
// ============================================================================

void pymyos_gui_draw_rect(void *surface, int x, int y, int w, int h, uint32_t color, int width) {
    extern void rust_gui_draw_rect(void *surface, int x, int y, int w, int h, uint32_t color, int width);
    rust_gui_draw_rect(surface, x, y, w, h, color, width);
}

void pymyos_gui_fill_rect(void *surface, int x, int y, int w, int h, uint32_t color) {
    extern void rust_gui_fill_rect(void *surface, int x, int y, int w, int h, uint32_t color);
    rust_gui_fill_rect(surface, x, y, w, h, color);
}

void pymyos_gui_draw_line(void *surface, int x1, int y1, int x2, int y2, uint32_t color, int width) {
    extern void rust_gui_draw_line(void *surface, int x1, int y1, int x2, int y2, uint32_t color, int width);
    rust_gui_draw_line(surface, x1, y1, x2, y2, color, width);
}

void pymyos_gui_draw_text(void *surface, int x, int y, const char *text, uint32_t color, int font_size) {
    extern void rust_gui_draw_text_on_surface(void *surface, int x, int y, const char *text, uint32_t color, int font_size);
    rust_gui_draw_text_on_surface(surface, x, y, text, color, font_size);
}

void pymyos_gui_draw_ellipse(void *surface, int cx, int cy, int rx, int ry, uint32_t color, int width) {
    extern void rust_gui_draw_ellipse(void *surface, int cx, int cy, int rx, int ry, uint32_t color, int width);
    rust_gui_draw_ellipse(surface, cx, cy, rx, ry, color, width);
}

int pymyos_gui_set_focus(uint64_t id) {
    extern int rust_gui_set_focus(uint64_t id);
    return rust_gui_set_focus(id);
}

uint64_t pymyos_gui_create_window(const char *title, int x, int y, int w, int h) {
    extern uint64_t rust_gui_create_window(const char *title, int x, int y, int w, int h);
    return rust_gui_create_window(title, x, y, w, h);
}

int pymyos_gui_render_frame(void) {
    extern int rust_gui_render_frame(void);
    return rust_gui_render_frame();
}

void pymyos_gui_init_desktop(int width, int height) {
    extern void rust_gui_init_desktop(int width, int height);
    rust_gui_init_desktop(width, height);
}

// ============================================================================
// Python runtime initialization (stub)
// ============================================================================

int pymyos_init(size_t heap_size) {
    (void)heap_size; // Unused in stub implementation
    if (pymyos_initialized) {
        return 0; // Already initialized
    }
    // In a real implementation, this would initialize MicroPython
    // For now, just mark as initialized
    pymyos_initialized = true;
    return 0;
}

void pymyos_deinit(void) {
    if (!pymyos_initialized) {
        return;
    }
    pymyos_initialized = false;
}

int pymyos_run_string(const char *code) {
    if (!pymyos_initialized) {
        return -1;
    }
    // In a real implementation, this would compile and execute Python code
    // For now, just print the code
    puts("[PYTHON] Executing: ");
    puts(code);
    putchar('\n');
    return 0;
}

int pymyos_run_file(const char *filename) {
    if (!pymyos_initialized) {
        return -1;
    }
    // In a real implementation, this would load and execute a Python file
    puts("[PYTHON] Running file: ");
    puts(filename);
    putchar('\n');
    return 0;
}

void pymyos_gc_collect(void) {
    // Stub
}

size_t pymyos_get_free_heap(void) {
    return 0;
}