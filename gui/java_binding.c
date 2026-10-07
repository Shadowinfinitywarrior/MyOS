/**
 * Java NativeGUI C Binding
 * Implements the native methods declared in myos.binding.NativeGUI
 * using kernel syscalls that forward to the Rust GUI system.
 */

#include <stdint.h>
#include <string.h>
#include "../include/rust_gui.h"
#include "../gui/rect.h"
#include "../kernel/syscall.h"

/* Forward declarations for syscall wrapper */
static inline int64_t do_syscall(int num, int64_t a1, int64_t a2, int64_t a3,
                                  int64_t a4, int64_t a5, int64_t a6) {
    register int64_t r10 __asm__("r10") = a4;
    register int64_t r8  __asm__("r8")  = a5;
    register int64_t r9  __asm__("r9")  = a6;
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (a1), "S" (a2), "d" (a3), "r" (r10), "r" (r8), "r" (r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* ============================================================
 * Native method implementations for NativeGUI
 * ============================================================ */

/**
 * Initialize the Rust GUI system.
 * @param width Screen width in pixels
 * @param height Screen height in pixels
 * @return 0 on success, negative on error
 */
int NativeGUI_init(int width, int height) {
    return (int)do_syscall(SYS_GUI_INIT, width, height, 0, 0, 0, 0);
}

/**
 * Create a new window.
 * @param title Window title (UTF-8)
 * @param x Initial X position
 * @param y Initial Y position
 * @param w Window width
 * @param h Window height
 * @return Window ID (0 on error)
 */
int64_t NativeGUI_createWindow(const char *title, int x, int y, int w, int h) {
    if (!title) return 0;
    return do_syscall(SYS_GUI_CREATE_WINDOW, (int64_t)title, x, y, w, h, 0);
}

/**
 * Destroy a window.
 * @param id Window ID to destroy
 */
void NativeGUI_destroyWindow(int64_t id) {
    do_syscall(SYS_GUI_DESTROY_WINDOW, id, 0, 0, 0, 0, 0);
}

/**
 * Invalidate a window (mark for redraw).
 * @param id Window ID to invalidate
 */
void NativeGUI_invalidateWindow(int64_t id) {
    do_syscall(SYS_GUI_INVALIDATE, id, 0, 0, 0, 0, 0);
}

/**
 * Render a frame (compose and display all windows).
 * @return 0 on success, negative on error
 */
int NativeGUI_renderFrame(void) {
    return (int)do_syscall(SYS_GUI_RENDER_FRAME, 0, 0, 0, 0, 0, 0);
}

/**
 * Set the framebuffer pointer (called from C kernel).
 * @param fb Pointer to framebuffer memory
 * @param width Framebuffer width
 * @param height Framebuffer height
 * @param pitch Framebuffer pitch (bytes per row)
 */
void NativeGUI_setFramebuffer(int64_t fb, int width, int height, int pitch) {
    do_syscall(SYS_GUI_SET_FRAMEBUFFER, fb, width, height, pitch, 0, 0);
}

/**
 * Get the number of active windows.
 * @return Window count
 */
int NativeGUI_windowCount(void) {
    return (int)do_syscall(SYS_GUI_WINDOW_COUNT, 0, 0, 0, 0, 0, 0);
}

/**
 * Get a window by index.
 * @param index Window index (0-based)
 * @return Pointer to window structure, or NULL if invalid
 */
struct window_t *NativeGUI_getWindow(int index) {
    return (struct window_t *)(uintptr_t)do_syscall(SYS_GUI_GET_WINDOW, index, 0, 0, 0, 0, 0);
}

/**
 * Focus a window.
 * @param id Window ID to focus
 * @return 1 on success, 0 on failure
 */
int NativeGUI_focusWindow(int64_t id) {
    return (int)do_syscall(SYS_GUI_FOCUS_WINDOW, id, 0, 0, 0, 0, 0);
}

/**
 * Set window title.
 * @param id Window ID
 * @param title New title
 */
void NativeGUI_setWindowTitle(int64_t id, const char *title) {
    if (!title) return;
    do_syscall(SYS_GUI_SET_WINDOW_TITLE, id, (int64_t)title, 0, 0, 0, 0);
}

/**
 * Get window frame rectangle.
 * @param id Window ID
 * @param rect Output rectangle
 * @return 1 on success, 0 on failure
 */
int NativeGUI_getWindowRect(int64_t id, rect_t *rect) {
    if (!rect) return 0;
    return (int)do_syscall(SYS_GUI_GET_WINDOW_RECT, id, (int64_t)rect, 0, 0, 0, 0);
}

/**
 * Set window frame rectangle (move/resize).
 * @param id Window ID
 * @param rect New rectangle
 * @return 1 on success, 0 on failure
 */
int NativeGUI_setWindowRect(int64_t id, const rect_t *rect) {
    if (!rect) return 0;
    return (int)do_syscall(SYS_GUI_SET_WINDOW_RECT, id, (int64_t)rect, 0, 0, 0, 0);
}

/**
 * Push a keyboard event to the Rust GUI event queue.
 * @param eventType Event type (EV_KEY_DOWN, EV_KEY_UP, EV_KEY_REPEAT)
 * @param scancode Hardware scancode
 * @param ascii ASCII character (0 if none)
 * @param modifiers Modifier flags (KMOD_SHIFT, KMOD_CTRL, etc.)
 */
void NativeGUI_pushKeyEvent(int eventType, int scancode, int ascii, int modifiers) {
    do_syscall(SYS_GUI_PUSH_KEY_EVENT, eventType, scancode, ascii, modifiers, 0, 0);
}

/**
 * Push a mouse event to the Rust GUI event queue.
 * @param eventType Event type (EV_MOUSE_MOVE, EV_MOUSE_DOWN, EV_MOUSE_UP, EV_SCROLL_VERTICAL, EV_SCROLL_HORIZONTAL)
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param button Button number (1=left, 2=middle, 3=right)
 * @param delta Scroll delta (for scroll events)
 */
void NativeGUI_pushMouseEvent(int eventType, int x, int y, int button, int delta) {
    do_syscall(SYS_GUI_PUSH_MOUSE_EVENT, eventType, x, y, button, delta, 0);
}