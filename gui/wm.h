#ifndef GUI_WM_H
#define GUI_WM_H

#include "surface.h"
#include "text.h"
#include "input.h"

/* Window manager.
 *
 * Windows are the unit of interaction. Each owns a surface, a frame rect, a
 * title, and a client callback that draws its content and handles events. The
 * manager owns z-order, focus, dragging, resizing, the maximize/minimize
 * states, and the decorations (title bar, close/min/max buttons, shadow).
 *
 * Hit-testing happens top-down, so the focused window naturally wins clicks
 * without any explicit routing table.
 */

#define WM_MAX_WINDOWS  16
#define WM_TITLEBAR_H    30
#define WM_BORDER_W       1
#define WM_SHADOW_PAD    10
#define WM_MIN_W        240
#define WM_MIN_H        120
#define WM_TITLE_MAX     64

#undef WF_MINIMIZED
#undef WF_MAXIMIZED
#undef WF_RESIZING
#undef WF_DRAGGING
#undef WF_NO_DECOR
#undef WF_MODAL

typedef enum {
    WF_MINIMIZED  = 1 << 0,
    WF_MAXIMIZED  = 1 << 1,
    WF_RESIZING   = 1 << 2,
    WF_DRAGGING   = 1 << 3,
    WF_NO_DECOR   = 1 << 4,
    WF_MODAL      = 1 << 5
} wm_flags_t;

/* Which resize edge/corner a grab started on. */
typedef enum {
    EDGE_NONE = 0,
    EDGE_N, EDGE_S, EDGE_E, EDGE_W,
    EDGE_NE, EDGE_NW, EDGE_SE, EDGE_SW
} wm_edge_t;

struct wm_window;

/* Client hooks. `paint` fills the client area of the window surface; `event`
 * receives everything not consumed by the decorations. Both are optional. */
typedef void (*wm_paint_fn)(struct wm_window *w, surface_t *s, const rect_t *client);
typedef bool (*wm_event_fn)(struct wm_window *w, const gui_event_t *e);
typedef void (*wm_close_fn)(struct wm_window *w);

typedef struct wm_window {
    char       title[WM_TITLE_MAX];
    rect_t     frame;          /* outer rect including title bar       */
    rect_t     restore;        /* frame saved across maximize           */
    rect_t     client;         /* content area, inside the frame        */
    surface_t *surf;
    int        flags;
    int        z;
    uint32_t   id;
    bool       visible;
    bool       focused;

    wm_paint_fn paint;
    wm_event_fn event;
    wm_close_fn on_close;
    void       *user;

    /* Drag/resize bookkeeping. */
    wm_edge_t  grab_edge;
    int        grab_px, grab_py;
    int        orig_x, orig_y, orig_w, orig_h;

    /* Cached painting state so we only repaint when something changed. */
    uint32_t    last_revision;
    bool        dirty;

    struct wm_window *next;    /* creation order, for taskbar listing  */
} wm_window_t;

void      wm_init(int screen_w, int screen_h);
void      wm_shutdown(void);

wm_window_t *wm_create(const char *title, int x, int y, int w, int h);
void      wm_destroy(wm_window_t *w);
void      wm_close(wm_window_t *w);
void      wm_focus(wm_window_t *w);
void      wm_invalidate(wm_window_t *w);
void      wm_invalidate_all(void);
void      wm_raise(wm_window_t *w);

int       wm_window_count(void);
wm_window_t *wm_window_at(int index);

/* The i-th window in creation order (stable); wm_window_at() is z-order. */
wm_window_t *wm_window_by_creation(int index);
wm_window_t *wm_find(const char *title);
wm_window_t *wm_focused(void);

/* True while a window is being dragged or resized, i.e. the WM is holding an
 * implicit mouse capture and wants mouse-move events forwarded to it. */
bool wm_capture_active(void);
wm_window_t *wm_first_of(void);

/* Client area in absolute screen coordinates (for cursor feedback etc). */
void      wm_client_screen_rect(const wm_window_t *w, rect_t *out);

/* Feed one event to the manager. Returns true if a window consumed it. */
bool      wm_handle_event(const gui_event_t *e);
/* Hit test without dispatching; used by the taskbar and desktop. */
wm_window_t *wm_window_at_point(int x, int y);

/* Draw the whole stack plus decorations into the back buffer and flush. */
void      wm_compose(void);
/* Client-area sub-rect the compositor should repaint for a damaged window. */
void      wm_window_damage(const wm_window_t *w);

/* Total screen area a window occupies, including its drop shadow. */
void      wm_visual_rect(const wm_window_t *w, rect_t *out);

/* Iterate for the taskbar: returns the i'th visible window in z order. */
int       wm_visible_count(void);
wm_window_t *wm_visible_at(int index);

/* Enhanced WM features */
void wm_cycle_next(void);           /* Alt+Tab: cycle forward */
void wm_cycle_prev(void);           /* Alt+Shift+Tab: cycle backward */
void wm_alt_tab_end(void);          /* End Alt+Tab mode */
void wm_snap_window(wm_window_t *w, int edge);  /* Win+Arrow: snap to edge */
void wm_toggle_maximize(wm_window_t *w);        /* Double-click or Win+Up */
void wm_tile_all(void);                         /* Tile all windows side-by-side */
void wm_minimize_all(void);         /* Win+D: show desktop */
void wm_restore_all(void);          /* Win+D again: restore */

#define WM_MAX_DESKTOPS 4
void wm_switch_desktop(int idx);    /* Super+Number */
int  wm_current_desktop(void);
void wm_move_to_desktop(wm_window_t *w, int idx);

/* External state */
extern int show_desktop_active;

#endif
