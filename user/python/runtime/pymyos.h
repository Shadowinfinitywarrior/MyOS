/*
 * MyOS Python Runtime - Header
 * Minimal syscall interface for Python bindings
 */

#ifndef PYMYOS_H
#define PYMYOS_H

#include <types.h>
#include <system.h>

// Python heap management
int pymyos_init(size_t heap_size);
void pymyos_deinit(void);

// Python execution
int pymyos_run_string(const char *code);
int pymyos_run_file(const char *filename);

// Garbage collection
void pymyos_gc_collect(void);
size_t pymyos_get_free_heap(void);

// Syscall interface
long pymyos_syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6);
void pymyos_exit(int code);
pid_t pymyos_fork(void);
pid_t pymyos_exec(const char *path);
int pymyos_read(int fd, void *buf, size_t count);
int pymyos_write(int fd, const void *buf, size_t count);
int pymyos_open(const char *path, int flags);
int pymyos_close(int fd);
pid_t pymyos_getpid(void);
void pymyos_sleep(unsigned int ms);
void pymyos_yield(void);
int pymyos_time(void);
int pymyos_uptime(void);
void pymyos_putchar(char c);
char pymyos_getchar(void);
void *pymyos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int pymyos_munmap(void *addr, size_t length);
int pymyos_shmget(const char *name, size_t size, int flags);
int pymyos_shmctl(int fd, int cmd, void *arg);

// GUI syscalls
void *pymyos_gui_create_surface(int width, int height);
int pymyos_gui_blit_surface(void *surface, int x, int y);
int pymyos_gui_invalidate(uint64_t id, int x, int y, int w, int h);
void pymyos_gui_get_fb_info(int *width, int *height, int *pitch);

// GUI drawing functions
void pymyos_gui_draw_rect(void *surface, int x, int y, int w, int h, uint32_t color, int width);
void pymyos_gui_fill_rect(void *surface, int x, int y, int w, int h, uint32_t color);
void pymyos_gui_draw_line(void *surface, int x1, int y1, int x2, int y2, uint32_t color, int width);
void pymyos_gui_draw_text(void *surface, int x, int y, const char *text, uint32_t color, int font_size);
void pymyos_gui_draw_ellipse(void *surface, int cx, int cy, int rx, int ry, uint32_t color, int width);
int pymyos_gui_set_focus(uint64_t id);
uint64_t pymyos_gui_create_window(const char *title, int x, int y, int w, int h);
int pymyos_gui_render_frame(void);
void pymyos_gui_init_desktop(int width, int height);

#endif // PYMYOS_H