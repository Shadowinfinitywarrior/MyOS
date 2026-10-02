#ifndef GUI_TERM_H
#define GUI_TERM_H

#include "wm.h"
#include "../kernel/vtty.h"

/* A terminal window bound to a virtual terminal.
 *
 * The window does not implement a shell. It renders a vty's character grid and
 * forwards keystrokes into that vty's input ring; the ring-3 shell writes its
 * output through the console fd, which the syscall layer routes to the same
 * vty. That is why the terminal needs no new syscalls to be useful.
 */

typedef struct term_window {
    vtty_t      *vty;
    uint32_t     last_revision;
    /* Blink phase for the cursor. */
    uint32_t     blink_tick;
    bool         cursor_on;
    int          scroll_top;   /* first visible row (for scrollback) */
} term_window_t;

wm_window_t *term_open(const char *title);
void         term_attach(wm_window_t *w, vtty_t *vty);
vtty_t      *term_vty_of(wm_window_t *w);
void         term_notify_dirty(wm_window_t *w);

/* Launch the real ring-3 shell into a new terminal window. */
wm_window_t *term_open_shell(void);

/* Repaint-on-change pass over every terminal window; called each frame by the
 * desktop. Registered through term_register_service so term.c does not depend
 * on desktop.c. */
void term_register_service(void);
extern void (*term_service_ptr)(void);

#endif
