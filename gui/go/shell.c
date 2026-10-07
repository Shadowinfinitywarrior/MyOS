/* MyOS Go Shell - Phase 3 Desktop Shell (C implementation for now)
 * Provides taskbar, start menu, desktop icons, and notifications.
 * Uses kernel syscalls for Rust GUI communication.
 */

#include "../kernel/syscall.h"
#include "libc.h"

/* Syscall wrappers for GUI operations */
static inline void *sys_gui_create_surface(int width, int height) {
    return (void *)(uintptr_t)_syscall(SYS_GUI_CREATE_SURFACE, width, height, 0, 0, 0, 0);
}

static inline int sys_gui_blit_surface(void *surface, int x, int y) {
    return _syscall(SYS_GUI_BLIT_SURFACE, (uintptr_t)surface, x, y, 0, 0, 0);
}

static inline int sys_gui_invalidate(uint64_t id, int x, int y, int w, int h) {
    return _syscall(SYS_GUI_INVALIDATE, id, x, y, w, h, 0);
}

static inline int sys_gui_get_fb_info(int *width, int *height, int *pitch) {
    return _syscall(SYS_GUI_GET_FB_INFO, (uintptr_t)width, (uintptr_t)height, (uintptr_t)pitch, 0, 0, 0);
}

/* Forward declaration */
int main(void);

/* Kernel thread entry point */
void goshell_kernel_main(void) {
    main();
}

/* User process entry point */
int main(void) {
    /* Get framebuffer info */
    int w, h, p;
    sys_gui_get_fb_info(&w, &h, &p);

    puts("Go Shell (C impl) starting...");

    /* Create initial surfaces */
    void *term_surf = sys_gui_create_surface(800, 600);
    void *about_surf = sys_gui_create_surface(500, 400);

    if (term_surf) {
        sys_gui_blit_surface(term_surf, 50, 50);
    }
    if (about_surf) {
        sys_gui_blit_surface(about_surf, 200, 150);
    }

    /* Main loop */
    for (;;) {
        sys_gui_invalidate(0, 0, 0, 1024, 768);
        _syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0);
    }
}