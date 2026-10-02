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
    desktop_run();
}

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

    wm_init((int)fb->width, (int)fb->height);
    cursor_init();
    desktop_init((int)fb->width, (int)fb->height);

    /* Open a couple of windows so the desktop is immediately meaningful:
     * a terminal with the real shell, and the About panel. */
    term_open_shell();
    desktop_launch("About");

    /* Focus the terminal so typing goes somewhere useful. */
    wm_window_t *term = wm_find("MyOS Terminal");
    if (term) wm_focus(term);

    kprintf("[GUI] Desktop running.\n");

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
