#include "desktop_boot.h"
#include "desktop.h"
#include "wm.h"
#include "term.h"
#include "cursor.h"
#include "input.h"
#include "../kernel/vtty.h"
#include "../drivers/framebuffer.h"
#include "../lib/printf.h"
#include "../kernel/process.h"
#include "../include/rust_gui.h"

/* Bring up the graphical session.
 *
 * Called once, after the framebuffer, keyboard and mouse are initialised. It
 * sets up the window manager, opens the first windows, and hands control to
 * the desktop loop. Processes already running (the init/spawned shell) are
 * bound to the boot terminal, so they keep writing to serial/VGA exactly as
 * before; only windows opened from here get their own terminals.
 */

/* Entry point for the desktop kernel process. Running the compositor as a
 * proper kernel process (rather than straight on the boot context's stack) is
 * what lets it share the CPU with the ring-3 shell: the shell's window only
 * repaints because the scheduler keeps round-robining back into here. */
static void desktop_main(void) {
    kprintf("[DESKTOP] desktop_main entered!\n");
    desktop_run();
}

/* Desktop boot - uses Rust GUI core for rendering, window management, and desktop shell.
 * C window manager and desktop are deprecated but kept for compatibility. */
void desktop_boot(void) {
    fb_info_t *fb = fb_get_info();
    if (!fb || fb->width == 0 || fb->height == 0) {
        kprintf("[GUI] No framebuffer; staying on the text console.\n");
        return;
    }
    if (!fb_get_backbuffer()) {
        kprintf("[GUI] No back buffer; staying on the text console.\n");
        return;
    }

    kprintf("[GUI] Starting desktop at %ux%u\n", fb->width, fb->height);

    /* Initialize Rust GUI system */
    rust_gui_init(fb->width, fb->height);
    rust_gui_set_framebuffer(fb_get_backbuffer(), fb->width, fb->height, fb->pitch);

    /* Initialize fonts from C baked font data */
    extern const uint16_t *ui_gw_ptr;
    extern const uint16_t *ui_gh_ptr;
    extern const int8_t *ui_xo_ptr;
    extern const int8_t *ui_yo_ptr;
    extern const uint16_t *ui_xa_ptr;
    extern const uint32_t *ui_bo_ptr;
    extern const uint8_t *ui_bitmap_ptr;
    extern const uint32_t ui_bitmap_size;
    
    extern const uint16_t *mono_gw_ptr;
    extern const uint16_t *mono_gh_ptr;
    extern const int8_t *mono_xo_ptr;
    extern const int8_t *mono_yo_ptr;
    extern const uint16_t *mono_xa_ptr;
    extern const uint32_t *mono_bo_ptr;
    extern const uint8_t *mono_bitmap_ptr;
    extern const uint32_t mono_bitmap_size;
    
    extern const uint16_t *ubold_gw_ptr;
    extern const uint16_t *ubold_gh_ptr;
    extern const int8_t *ubold_xo_ptr;
    extern const int8_t *ubold_yo_ptr;
    extern const uint16_t *ubold_xa_ptr;
    extern const uint32_t *ubold_bo_ptr;
    extern const uint8_t *ubold_bitmap_ptr;
    extern const uint32_t ubold_bitmap_size;
    
    extern const uint16_t *blocks_gw_ptr;
    extern const uint16_t *blocks_gh_ptr;
    extern const int8_t *blocks_xo_ptr;
    extern const int8_t *blocks_yo_ptr;
    extern const uint16_t *blocks_xa_ptr;
    extern const uint32_t *blocks_bo_ptr;
    extern const uint8_t *blocks_bitmap_ptr;
    extern const uint32_t blocks_bitmap_size;
    
    rust_gui_init_fonts(
        ui_gw_ptr, ui_gh_ptr, ui_xo_ptr, ui_yo_ptr, ui_xa_ptr, ui_bo_ptr, ui_bitmap_ptr, ui_bitmap_size,
        mono_gw_ptr, mono_gh_ptr, mono_xo_ptr, mono_yo_ptr, mono_xa_ptr, mono_bo_ptr, mono_bitmap_ptr, mono_bitmap_size,
        ubold_gw_ptr, ubold_gh_ptr, ubold_xo_ptr, ubold_yo_ptr, ubold_xa_ptr, ubold_bo_ptr, ubold_bitmap_ptr, ubold_bitmap_size,
        blocks_gw_ptr, blocks_gh_ptr, blocks_xo_ptr, blocks_yo_ptr, blocks_xa_ptr, blocks_bo_ptr, blocks_bitmap_ptr, blocks_bitmap_size
    );

    /* Initialize C cursor (still used for drawing) */
    cursor_init();
    
    /* Initialize C window manager (for legacy app compatibility) */
    wm_init((int)fb->width, (int)fb->height);
    
    /* Initialize C desktop for shell elements (taskbar, icons, menu) - deprecated */
    desktop_init((int)fb->width, (int)fb->height);

    /* Create initial C-side windows for shell apps */
    term_open_shell();
    desktop_launch("About");

    /* Focus the terminal so typing goes somewhere useful. */
    wm_window_t *term = wm_find("MyOS Terminal");
    if (term) wm_focus(term);

    kprintf("[GUI] Desktop running with %d windows\n", wm_window_count());

    /* The boot console stops mirroring to the framebuffer from here; the
     * desktop owns the screen. */
    process_t *desk = process_create_kernel("desktop", desktop_main);
    if (!desk) {
        kprintf("[GUI] could not create desktop process; running inline.\n");
        desktop_run();
        return;
    }

    /* Hand the CPU over for good. The boot context has no saved context of
     * its own, so yielding from it is one-way; from here the timer-driven
     * scheduler alternates the desktop with the ring-3 shell. */
    process_yield();

    /* Only reached if nothing is runnable, which would mean no scheduler. */
    for (;;) __asm__ volatile("hlt");
}