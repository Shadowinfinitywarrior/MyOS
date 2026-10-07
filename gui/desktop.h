#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include "wm.h"

/* The desktop shell: wallpaper, taskbar, start menu, desktop icons and the
 * app registry. It owns the screen background and the taskbar, and dispatches
 * launches to the registered applications.
 */

typedef void (*app_launch_fn)(void);

typedef struct app_entry {
    const char   *name;
    const char   *desc;
    app_launch_fn launch;
    const uint8_t *icon;   /* 16x16 1bpp, row-stride 2 bytes */
} app_entry_t;

#define DESKTOP_MAX_ICONS 16

void desktop_init(int w, int h);
void desktop_shutdown(void);

/* One iteration: pump input, let the shell react, composite, flush. */
void desktop_tick(void);
void desktop_run(void);      /* never returns */

/* Repaint everything (used after a window opens/closes/moves). */
void desktop_invalidate(void);
void desktop_invalidate_rect(const rect_t *r);

/* Taskbar geometry, so the WM can keep windows above the bar. */
int  desktop_workarea_bottom(void);

/* Launch an app by name, or list the registry. */
bool desktop_launch(const char *name);
int  desktop_app_count(void);
const app_entry_t *desktop_app_at(int i);

/* The clock, exposed for the taskbar and for apps that want a timestamp. */
void desktop_format_clock(char *buf, int len, bool with_seconds);
void desktop_set_theme(const char *name);

#endif
