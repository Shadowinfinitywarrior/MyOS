#include "wm.h"
#include "theme.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "../kernel/timer.h"
#include "../drivers/framebuffer.h"

/* ---- state -------------------------------------------------------------- */

static wm_window_t *windows[WM_MAX_WINDOWS];   /* back == topmost (z order) */
static int          win_count;
static wm_window_t *focus;
static int          screen_w, screen_h;
static uint32_t     next_id = 1;
static uint32_t     last_click_time;
static int          last_click_x, last_click_y;

/* Enhanced WM state */
static int alt_tab_active = 0;
static wm_window_t *alt_tab_candidates[WM_MAX_WINDOWS];
static int alt_tab_count = 0;
static int alt_tab_index = 0;

static int current_desktop = 0;
static wm_window_t *desktop_windows[WM_MAX_DESKTOPS][WM_MAX_WINDOWS];
static int desktop_win_count[WM_MAX_DESKTOPS] = {0};

static int show_desktop_prev_desktop = -1;
int show_desktop_active = 0;

/* A window's surface covers the WHOLE frame, title bar included. Keeping the
 * decorations inside the surface means every coordinate the window manager
 * draws with is surface-local, and presenting a window is a single blit. The
 * client area is then just a sub-rect of that surface handed to the app. */
static void client_in_surface(const wm_window_t *w, rect_t *out) {
    if (w->flags & WF_NO_DECOR) { *out = w->frame; out->x = out->y = 0; return; }
    out->x = WM_BORDER_W;
    out->y = WM_BORDER_W + WM_TITLEBAR_H;
    out->w = w->frame.w - WM_BORDER_W * 2;
    out->h = w->frame.h - WM_BORDER_W * 2 - WM_TITLEBAR_H;
}

/* Client area in absolute screen coordinates. */
static void client_on_screen(const wm_window_t *w, rect_t *out) {
    client_in_surface(w, out);
    out->x += w->frame.x;
    out->y += w->frame.y;
}

void wm_client_screen_rect(const wm_window_t *w, rect_t *out) {
    if (w) client_on_screen(w, out);
}

void wm_visual_rect(const wm_window_t *w, rect_t *out) {
    *out = w->frame;
    if (w->flags & WF_MAXIMIZED) return;
    rect_inset(out, -WM_SHADOW_PAD, -WM_SHADOW_PAD);
}

void wm_window_damage(const wm_window_t *w) {
    rect_t vr;
    wm_visual_rect(w, &vr);
    rect_t clip = { 0, 0, screen_w, screen_h };
    rect_t vis;
    if (rect_intersect(&vr, &clip, &vis))
        fb_add_damage(vis.x, vis.y, vis.w, vis.h);
}

/* ---- lifecycle ---------------------------------------------------------- */

void wm_init(int w, int h) {
    screen_w = w;
    screen_h = h;
    win_count = 0;
    focus = NULL;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) windows[i] = NULL;
}

void wm_shutdown(void) {
    for (int i = 0; i < win_count; i++) {
        if (windows[i] && windows[i]->surf) surface_destroy(windows[i]->surf);
    }
    win_count = 0;
    focus = NULL;
}

static void sync_surface(wm_window_t *w) {
    if (w->surf && w->surf->w == w->frame.w && w->surf->h == w->frame.h) return;
    surface_t *ns = surface_create(w->frame.w, w->frame.h);
    if (!ns) return;
    if (w->surf) surface_destroy(w->surf);
    w->surf = ns;
    w->dirty = true;
}

wm_window_t *wm_create(const char *title, int x, int y, int w, int h) {
    if (win_count >= WM_MAX_WINDOWS) return NULL;
    if (w < WM_MIN_W) w = WM_MIN_W;
    if (h < WM_MIN_H + WM_TITLEBAR_H) h = WM_MIN_H + WM_TITLEBAR_H;
    if (w > screen_w) w = screen_w;
    if (h > screen_h) h = screen_h;

    wm_window_t *win = kzalloc(sizeof(wm_window_t));
    if (!win) return NULL;

    win->frame.x = x;
    win->frame.y = y;
    win->frame.w = w;
    win->frame.h = h;
    win->restore = win->frame;
    win->id = next_id++;
    win->visible = true;
    win->dirty = true;
    if (title) {
        int n = (int)strlen(title);
        if (n > WM_TITLE_MAX - 1) n = WM_TITLE_MAX - 1;
        for (int i = 0; i < n; i++) win->title[i] = title[i];
        win->title[n] = 0;
    }
    win->surf = surface_create(w, h);
    if (!win->surf) { kfree(win); return NULL; }

    windows[win_count++] = win;
    wm_focus(win);
    return win;
}

void wm_destroy(wm_window_t *w) {
    if (!w) return;
    int idx = -1;
    for (int i = 0; i < win_count; i++) if (windows[i] == w) { idx = i; break; }
    if (idx < 0) return;
    if (focus == w) {
        /* Focus falls to the window underneath, or the new topmost. */
        focus = (idx > 0) ? windows[idx - 1] : NULL;
    }
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    win_count--;
    if (focus && focus->focused) { focus->focused = true; focus->dirty = true; }
    if (w->surf) surface_destroy(w->surf);
    kfree(w);
    if (focus) wm_invalidate(focus);
}

void wm_close(wm_window_t *w) {
    if (!w) return;
    if (w->on_close) w->on_close(w);
    wm_destroy(w);
}

void wm_invalidate(wm_window_t *w) { if (w) w->dirty = true; }

void wm_invalidate_all(void) {
    for (int i = 0; i < win_count; i++) windows[i]->dirty = true;
}

void wm_raise(wm_window_t *w) {
    if (!w) return;
    int idx = -1;
    for (int i = 0; i < win_count; i++) if (windows[i] == w) { idx = i; break; }
    if (idx < 0 || idx == win_count - 1) return;
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    windows[win_count - 1] = w;
}

int wm_window_count(void) { return win_count; }

wm_window_t *wm_window_at(int index) {
    if (index < 0 || index >= win_count) return NULL;
    return windows[index];
}

/* The i-th window in creation order.
 *
 * wm_window_at() indexes z-order, and raising a window moves it to the end of
 * the array, so any UI that keys off it (taskbar buttons, say) reshuffles and
 * can end up pointing at a different window after an unrelated focus change.
 * Ordering by id is stable for the lifetime of the window. */
wm_window_t *wm_window_by_creation(int index) {
    if (index < 0) return NULL;
    for (int i = 0; i < win_count; i++) {
        int rank = 0;
        for (int j = 0; j < win_count; j++)
            if (windows[j]->id < windows[i]->id) rank++;
        if (rank == index) return windows[i];
    }
    return NULL;
}

wm_window_t *wm_find(const char *title) {
    if (!title) return NULL;
    for (int i = 0; i < win_count; i++)
        if (strcmp(windows[i]->title, title) == 0) return windows[i];
    return NULL;
}

wm_window_t *wm_focused(void) { return focus; }

bool wm_capture_active(void) {
    for (int i = 0; i < win_count; i++)
        if (windows[i]->flags & (WF_DRAGGING | WF_RESIZING)) return true;
    return false;
}
wm_window_t *wm_first_of(void) { return win_count ? windows[0] : NULL; }

int wm_visible_count(void) {
    int n = 0;
    for (int i = 0; i < win_count; i++)
        if (windows[i]->visible && !(windows[i]->flags & WF_MINIMIZED)) n++;
    return n;
}

wm_window_t *wm_visible_at(int index) {
    int n = 0;
    for (int i = 0; i < win_count; i++) {
        wm_window_t *w = windows[i];
        if (!w->visible || (w->flags & WF_MINIMIZED)) continue;
        if (n == index) return w;
        n++;
    }
    return NULL;
}

/* ---- hit testing -------------------------------------------------------- */

wm_window_t *wm_window_at_point(int x, int y) {
    for (int i = win_count - 1; i >= 0; i--) {
        wm_window_t *w = windows[i];
        if (!w->visible || (w->flags & WF_MINIMIZED)) continue;
        if (rect_contains_point(&w->frame, x, y)) return w;
    }
    return NULL;
}

typedef enum { HIT_CLIENT, HIT_TITLE, HIT_CLOSE, HIT_MIN, HIT_MAX, HIT_EDGE } hit_t;

/* Modern traffic-light buttons on the top-left of the titlebar */
#define BTN_SIZE 12
#define BTN_GAP  8
#define BTN_START_X 14

static int btn_x(const wm_window_t *w, int slot) {
    /* slot 0 = close, 1 = maximize, 2 = minimize */
    int x0 = w->frame.x + BTN_START_X;
    if (slot == 0) return x0;
    if (slot == 2) return x0 + (BTN_SIZE + BTN_GAP);
    return x0 + (BTN_SIZE + BTN_GAP) * 2;
}

static hit_t hit_test(const wm_window_t *w, int x, int y, wm_edge_t *edge_out) {
    if (edge_out) *edge_out = EDGE_NONE;
    if (!rect_contains_point(&w->frame, x, y)) return HIT_CLIENT;
    if (w->flags & WF_NO_DECOR) return HIT_CLIENT;

    /* The title bar stays hit-testable while maximized */
    int tb_y = w->frame.y + WM_BORDER_W;
    if (y >= tb_y && y < tb_y + WM_TITLEBAR_H) {
        for (int slot = 0; slot < 3; slot++) {
            int bx = btn_x(w, slot);
            int by = tb_y + (WM_TITLEBAR_H - BTN_SIZE) / 2;
            if (x >= bx - 3 && x < bx + BTN_SIZE + 3 && y >= by - 3 && y < by + BTN_SIZE + 3)
                return slot == 0 ? HIT_CLOSE : (slot == 1 ? HIT_MAX : HIT_MIN);
        }
        return HIT_TITLE;
    }

    /* A maximized window already fills the work area, so it has no edges left
     * to grab; only the title bar above is interactive. */
    if (w->flags & WF_MAXIMIZED) return HIT_CLIENT;

    /* Resize edges just inside the frame; corners take priority over sides. */
    int g = 6;
    int fx0 = w->frame.x, fy0 = w->frame.y;
    int fx1 = w->frame.x + w->frame.w, fy1 = w->frame.y + w->frame.h;
    bool near_l = x < fx0 + g, near_r = x >= fx1 - g;
    bool near_t = y < fy0 + g, near_b = y >= fy1 - g;
    bool in_h = x >= fx0 - g && x <= fx1 + g;
    bool in_v = y >= fy0 - g && y <= fy1 + g;
    if (in_h && in_v) {
        if (near_t && near_l) { if (edge_out) *edge_out = EDGE_NW; return HIT_EDGE; }
        if (near_t && near_r) { if (edge_out) *edge_out = EDGE_NE; return HIT_EDGE; }
        if (near_b && near_l) { if (edge_out) *edge_out = EDGE_SW; return HIT_EDGE; }
        if (near_b && near_r) { if (edge_out) *edge_out = EDGE_SE; return HIT_EDGE; }
        if (near_t) { if (edge_out) *edge_out = EDGE_N; return HIT_EDGE; }
        if (near_b) { if (edge_out) *edge_out = EDGE_S; return HIT_EDGE; }
        if (near_l) { if (edge_out) *edge_out = EDGE_W; return HIT_EDGE; }
        if (near_r) { if (edge_out) *edge_out = EDGE_E; return HIT_EDGE; }
    }
    return HIT_CLIENT;
}

static void clamp_to_screen(wm_window_t *w) {
    if (w->frame.w > screen_w) w->frame.w = screen_w;
    if (w->frame.h > screen_h) w->frame.h = screen_h;
    if (w->frame.x > screen_w - 60) w->frame.x = screen_w - 60;
    if (w->frame.y > screen_h - WM_TITLEBAR_H - 4) w->frame.y = screen_h - WM_TITLEBAR_H - 4;
    if (w->frame.x < -w->frame.w + 60) w->frame.x = -w->frame.w + 60;
    if (w->frame.y < 0) w->frame.y = 0;
}

static void toggle_maximize(wm_window_t *w) {
    if (w->flags & WF_MAXIMIZED) {
        w->frame = w->restore;
        w->flags &= ~WF_MAXIMIZED;
    } else {
        w->restore = w->frame;
        w->frame.x = 0;
        w->frame.y = 0;
        w->frame.w = screen_w;
        w->frame.h = screen_h;
        w->flags |= WF_MAXIMIZED;
    }
    sync_surface(w);
    wm_invalidate(w);
}

void wm_focus(wm_window_t *w) {
    if (focus == w) { if (w) wm_raise(w); return; }
    if (focus) { focus->focused = false; focus->dirty = true; }
    focus = w;
    if (w) { w->focused = true; w->dirty = true; wm_raise(w); }
}

/* ---- event dispatch ------------------------------------------------------ */

static void apply_resize(wm_window_t *w, wm_edge_t e, int dx, int dy) {
    int x = w->frame.x, y = w->frame.y, ww = w->frame.w, hh = w->frame.h;
    switch (e) {
        case EDGE_N:  y += dy; hh -= dy; break;
        case EDGE_S:  hh += dy; break;
        case EDGE_E:  ww += dx; break;
        case EDGE_W:  x += dx; ww -= dx; break;
        case EDGE_NE: y += dy; hh -= dy; ww += dx; break;
        case EDGE_NW: y += dy; hh -= dy; x += dx; ww -= dx; break;
        case EDGE_SE: hh += dy; ww += dx; break;
        case EDGE_SW: hh += dy; x += dx; ww -= dx; break;
        default: return;
    }
    if (ww < WM_MIN_W) {
        if (e == EDGE_W || e == EDGE_NW || e == EDGE_SW) x -= (WM_MIN_W - ww);
        ww = WM_MIN_W;
    }
    if (hh < WM_MIN_H + WM_TITLEBAR_H) {
        if (e == EDGE_N || e == EDGE_NE || e == EDGE_NW) y -= (WM_MIN_H + WM_TITLEBAR_H - hh);
        hh = WM_MIN_H + WM_TITLEBAR_H;
    }
    w->frame.x = x; w->frame.y = y; w->frame.w = ww; w->frame.h = hh;
    sync_surface(w);
    wm_invalidate(w);
}

bool wm_handle_event(const gui_event_t *e) {
    if (!e) return false;

    if (e->type == EV_MOUSE_DOWN) {
        wm_window_t *w = wm_window_at_point(e->x, e->y);
        if (!w) return false;
        wm_focus(w);

        wm_edge_t edge;
        hit_t hit = hit_test(w, e->x, e->y, &edge);
        switch (hit) {
            case HIT_CLOSE:
                wm_destroy(w);
                return true;
            case HIT_MIN:
                w->flags |= WF_MINIMIZED;
                wm_invalidate(w);
                return true;
            case HIT_MAX:
                toggle_maximize(w);
                return true;
            case HIT_TITLE: {
                bool dbl = (e->x - last_click_x) < 6 && (e->x - last_click_x) > -6 &&
                           (e->y - last_click_y) < 6 && (e->y - last_click_y) > -6 &&
                           (e->timestamp - last_click_time) < 500;
                last_click_time = e->timestamp;
                last_click_x = e->x;
                last_click_y = e->y;
                if (dbl) { toggle_maximize(w); return true; }
                if (w->flags & WF_MAXIMIZED) return true;
                w->flags |= WF_DRAGGING;
                w->grab_px = e->x;
                w->grab_py = e->y;
                w->orig_x = w->frame.x;
                w->orig_y = w->frame.y;
                return true;
            }
            case HIT_EDGE:
                if (w->flags & WF_MAXIMIZED) return true;
                w->flags |= WF_RESIZING;
                w->grab_edge = edge;
                w->grab_px = e->x;
                w->grab_py = e->y;
                return true;
            case HIT_CLIENT:
            default:
                if (w->event) w->event(w, e);
                return true;
        }
    }

    if (e->type == EV_MOUSE_MOVE) {
        wm_window_t *w = wm_window_at_point(e->x, e->y);
        if (w && (w->flags & WF_MINIMIZED)) w = NULL;
        if (!w) {
            /* Still let a dragging window follow the pointer past its edge. */
            for (int i = 0; i < win_count; i++) {
                if (windows[i]->flags & (WF_DRAGGING | WF_RESIZING)) {
                    w = windows[i];
                    break;
                }
            }
            if (!w) return false;
        }
        if (w->flags & WF_DRAGGING) {
            w->frame.x = w->orig_x + (e->x - w->grab_px);
            w->frame.y = w->orig_y + (e->y - w->grab_py);
            clamp_to_screen(w);
            wm_invalidate(w);
            return true;
        }
        if (w->flags & WF_RESIZING) {
            apply_resize(w, w->grab_edge, e->x - w->grab_px, e->y - w->grab_py);
            return true;
        }
        if (w->event) return w->event(w, e);
        return false;
    }

    if (e->type == EV_MOUSE_UP) {
        for (int i = 0; i < win_count; i++) {
            wm_window_t *w = windows[i];
            if (w->flags & (WF_DRAGGING | WF_RESIZING)) {
                w->flags &= ~(WF_DRAGGING | WF_RESIZING);
                w->grab_edge = EDGE_NONE;
                wm_invalidate(w);
                return true;
            }
        }
        wm_window_t *w = wm_window_at_point(e->x, e->y);
        if (w && w->event) return w->event(w, e);
        return false;
    }

    if (e->type == EV_MOUSE_SCROLL) {
        wm_window_t *w = wm_window_at_point(e->x, e->y);
        if (!w) return false;
        wm_focus(w);
        if (w->event) return w->event(w, e);
        return false;
    }

    if (e->type == EV_KEY_DOWN) {
        if (!focus || (focus->flags & WF_MINIMIZED)) return false;
        if (focus->event) return focus->event(focus, e);
        return false;
    }

    return false;
}

/* ---- painting ------------------------------------------------------------ */


static void draw_decorations(wm_window_t *w) {
    surface_t *s = w->surf;
    if (!s) return;

    color_t border = w->focused ? TH_BORDER_FOCUS : TH_BORDER;
    color_t bar    = w->focused ? TH_TITLE_FOCUS : TH_TITLE;
    color_t text   = w->focused ? TH_TITLE_TEXT : TH_TITLE_TEXT_DIM;

    /* Frame border: 1px precision modern border */
    rect_t r = { 0, 0, w->frame.w, w->frame.h };
    surface_rect_outline(s, &r, border, 1);

    if (w->flags & WF_NO_DECOR) return;

    /* Title bar: acrylic dark styling */
    rect_t tb = { 1, 1, w->frame.w - 2, WM_TITLEBAR_H };
    surface_fill_rect(s, &tb, bar);

    /* 1px top highlight and bottom separator line */
    rect_t top_edge = { 1, 1, w->frame.w - 2, 1 };
    surface_fill_rect(s, &top_edge, w->focused ? RGB(0x36, 0x3D, 0x4B) : RGB(0x28, 0x2D, 0x36));
    rect_t bot_edge = { 1, WM_TITLEBAR_H, w->frame.w - 2, 1 };
    surface_fill_rect(s, &bot_edge, RGB(0x1B, 0x1F, 0x27));

    /* Traffic-light buttons on the left */
    int by = tb.y + (tb.h - BTN_SIZE) / 2;
    int x_close = 1 + BTN_START_X;
    int x_min   = x_close + (BTN_SIZE + BTN_GAP);
    int x_max   = x_min + (BTN_SIZE + BTN_GAP);

    /* Close (Coral Red) */
    rect_t r_close = { x_close, by, BTN_SIZE, BTN_SIZE };
    surface_rounded_fill(s, &r_close, 6, w->focused ? TH_BTN_CLOSE : RGB(0x48, 0x4F, 0x58));
    surface_rounded_outline(s, &r_close, 6, w->focused ? RGB(0xE0, 0x44, 0x3E) : RGB(0x38, 0x3E, 0x47), 1);

    /* Minimize (Amber) */
    rect_t r_min = { x_min, by, BTN_SIZE, BTN_SIZE };
    surface_rounded_fill(s, &r_min, 6, w->focused ? TH_BTN_MIN : RGB(0x48, 0x4F, 0x58));
    surface_rounded_outline(s, &r_min, 6, w->focused ? RGB(0xDE, 0xA1, 0x23) : RGB(0x38, 0x3E, 0x47), 1);

    /* Maximize (Emerald) */
    rect_t r_max = { x_max, by, BTN_SIZE, BTN_SIZE };
    surface_rounded_fill(s, &r_max, 6, w->focused ? TH_BTN_MAX : RGB(0x48, 0x4F, 0x58));
    surface_rounded_outline(s, &r_max, 6, w->focused ? RGB(0x1A, 0xAB, 0x29) : RGB(0x38, 0x3E, 0x47), 1);

    /* Centered crisp title */
    int tw = text_width(font_bold(), w->title);
    int tx = (w->frame.w - tw) / 2;
    if (tx < x_max + BTN_SIZE + 16) tx = x_max + BTN_SIZE + 16;
    int ty = tb.y + (tb.h - 15) / 2;
    text_draw_n(s, font_bold(), tx, ty, w->title, w->frame.w - tx - 16, text);
}

void wm_compose(void) {
    for (int i = 0; i < win_count; i++) {
        wm_window_t *w = windows[i];
        if (!w->visible || (w->flags & WF_MINIMIZED)) continue;

        if (w->dirty || !w->surf) {
            rect_t client;
            client_in_surface(w, &client);
            if (w->paint && w->surf) w->paint(w, w->surf, &client);
            draw_decorations(w);
            w->dirty = false;
        }

        /* Soft ambient drop shadow beneath the window */
        uint32_t *bb = fb_get_backbuffer();
        if (bb && !(w->flags & WF_MAXIMIZED)) {
            int stride = fb_get_stride();
            rect_t outer = { w->frame.x - WM_SHADOW_PAD + 2, w->frame.y - WM_SHADOW_PAD + 4,
                             w->frame.w + WM_SHADOW_PAD * 2, w->frame.h + WM_SHADOW_PAD * 2 };
            rect_t screen = { 0, 0, screen_w, screen_h };
            rect_t vis;
            if (rect_intersect(&outer, &screen, &vis)) {
                for (int y = vis.y; y < vis.y + vis.h; y++) {
                    uint32_t *row = bb + (size_t)y * (uint32_t)stride;
                    for (int x = vis.x; x < vis.x + vis.w; x++) {
                        bool inside_frame = x >= w->frame.x && x < w->frame.x + w->frame.w &&
                                            y >= w->frame.y && y < w->frame.y + w->frame.h;
                        if (inside_frame) continue;
                        int dx = x < w->frame.x ? w->frame.x - x
                               : (x >= w->frame.x + w->frame.w ? x - (w->frame.x + w->frame.w - 1) : 0);
                        int dy = y < w->frame.y ? w->frame.y - y
                               : (y >= w->frame.y + w->frame.h ? y - (w->frame.y + w->frame.h - 1) : 0);
                        int d;
                        if (dx > 0 && dy > 0) {
                            d = (dx > dy) ? (dx + (dy * 3) / 8) : (dy + (dx * 3) / 8);
                        } else {
                            d = dx > dy ? dx : dy;
                        }
                        if (d > WM_SHADOW_PAD) continue;
                        uint32_t c = row[x];
                        uint32_t rem = (uint32_t)(WM_SHADOW_PAD - d);
                        uint32_t alpha = (rem * rem * 140) / (WM_SHADOW_PAD * WM_SHADOW_PAD);
                        uint32_t inv = 256 - alpha;
                        uint32_t r = (((c >> 16) & 0xFF) * inv) >> 8;
                        uint32_t g = (((c >> 8) & 0xFF) * inv) >> 8;
                        uint32_t b = ((c & 0xFF) * inv) >> 8;
                        row[x] = (r << 16) | (g << 8) | b;
                    }
                }
                fb_add_damage(vis.x, vis.y, vis.w, vis.h);
            }
        }

        if (w->surf) surface_present(w->surf, w->frame.x, w->frame.y, NULL);
    }
}

/* ---- Enhanced WM features -------------------------------------------------- */

/* Build list of visible, non-minimized windows on current desktop for Alt+Tab */
static void build_alt_tab_list(void) {
    alt_tab_count = 0;
    for (int i = win_count - 1; i >= 0; i--) {
        wm_window_t *w = windows[i];
        if (w->visible && !(w->flags & WF_MINIMIZED)) {
            alt_tab_candidates[alt_tab_count++] = w;
            if (alt_tab_count >= WM_MAX_WINDOWS) break;
        }
    }
}

/* Alt+Tab: cycle to next window */
void wm_cycle_next(void) {
    if (!alt_tab_active) {
        build_alt_tab_list();
        if (alt_tab_count == 0) return;
        alt_tab_active = 1;
        alt_tab_index = 0;
    } else {
        alt_tab_index = (alt_tab_index + 1) % alt_tab_count;
    }
    if (alt_tab_index < alt_tab_count) {
        wm_focus(alt_tab_candidates[alt_tab_index]);
        wm_raise(alt_tab_candidates[alt_tab_index]);
    }
}

/* Alt+Shift+Tab: cycle to previous window */
void wm_cycle_prev(void) {
    if (!alt_tab_active) {
        build_alt_tab_list();
        if (alt_tab_count == 0) return;
        alt_tab_active = 1;
        alt_tab_index = 0;
    } else {
        alt_tab_index = (alt_tab_index - 1 + alt_tab_count) % alt_tab_count;
    }
    if (alt_tab_index < alt_tab_count) {
        wm_focus(alt_tab_candidates[alt_tab_index]);
        wm_raise(alt_tab_candidates[alt_tab_index]);
    }
}

/* End Alt+Tab mode (called on Alt release) */
void wm_alt_tab_end(void) {
    alt_tab_active = 0;
    alt_tab_count = 0;
    alt_tab_index = 0;
}

/* Snap window to screen edge (Win+Arrow) */
void wm_snap_window(wm_window_t *w, int edge) {
    if (!w || (w->flags & (WF_MAXIMIZED | WF_MINIMIZED))) return;
    
    int half_w = screen_w / 2;
    int half_h = screen_h / 2;
    
    switch (edge) {
        case EDGE_W:  /* Left half */
            w->frame.x = 0;
            w->frame.y = 0;
            w->frame.w = half_w;
            w->frame.h = screen_h;
            break;
        case EDGE_E:  /* Right half */
            w->frame.x = half_w;
            w->frame.y = 0;
            w->frame.w = half_w;
            w->frame.h = screen_h;
            break;
        case EDGE_N:  /* Top half */
            w->frame.x = 0;
            w->frame.y = 0;
            w->frame.w = screen_w;
            w->frame.h = half_h;
            break;
        case EDGE_S:  /* Bottom half */
            w->frame.x = 0;
            w->frame.y = half_h;
            w->frame.w = screen_w;
            w->frame.h = half_h;
            break;
        case EDGE_NW: /* Top-left quarter */
            w->frame.x = 0;
            w->frame.y = 0;
            w->frame.w = half_w;
            w->frame.h = half_h;
            break;
        case EDGE_NE: /* Top-right quarter */
            w->frame.x = half_w;
            w->frame.y = 0;
            w->frame.w = half_w;
            w->frame.h = half_h;
            break;
        case EDGE_SW: /* Bottom-left quarter */
            w->frame.x = 0;
            w->frame.y = half_h;
            w->frame.w = half_w;
            w->frame.h = half_h;
            break;
        case EDGE_SE: /* Bottom-right quarter */
            w->frame.x = half_w;
            w->frame.y = half_h;
            w->frame.w = half_w;
            w->frame.h = half_h;
            break;
    }
    sync_surface(w);
    wm_invalidate(w);
}

/* Toggle maximize state */
void wm_toggle_maximize(wm_window_t *w) {
    if (!w) return;
    if (w->flags & WF_MAXIMIZED) {
        w->frame = w->restore;
        w->flags &= ~WF_MAXIMIZED;
    } else {
        w->restore = w->frame;
        w->frame.x = 0;
        w->frame.y = 0;
        w->frame.w = screen_w;
        w->frame.h = screen_h;
        w->flags |= WF_MAXIMIZED;
    }
    sync_surface(w);
    wm_invalidate(w);
}

/* Minimize all windows (Win+D) */
void wm_minimize_all(void) {
    if (show_desktop_active) return;
    show_desktop_active = 1;
    show_desktop_prev_desktop = current_desktop;
    for (int i = 0; i < win_count; i++) {
        if (windows[i]->visible && !(windows[i]->flags & WF_MINIMIZED)) {
            windows[i]->flags |= WF_MINIMIZED;
            wm_invalidate(windows[i]);
        }
    }
}

/* Restore all minimized windows */
void wm_restore_all(void) {
    if (!show_desktop_active) return;
    for (int i = 0; i < win_count; i++) {
        if (windows[i]->flags & WF_MINIMIZED) {
            windows[i]->flags &= ~WF_MINIMIZED;
            wm_invalidate(windows[i]);
        }
    }
    show_desktop_active = 0;
    show_desktop_prev_desktop = -1;
}

/* Virtual desktop support */
void wm_switch_desktop(int idx) {
    if (idx < 0 || idx >= WM_MAX_DESKTOPS || idx == current_desktop) return;
    
    /* Save current desktop windows */
    desktop_win_count[current_desktop] = win_count;
    for (int i = 0; i < win_count; i++) {
        desktop_windows[current_desktop][i] = windows[i];
    }
    
    /* Load target desktop windows */
    current_desktop = idx;
    win_count = desktop_win_count[current_desktop];
    for (int i = 0; i < win_count; i++) {
        windows[i] = desktop_windows[current_desktop][i];
    }
    
    focus = NULL;
    for (int i = win_count - 1; i >= 0; i--) {
        if (windows[i]->visible && !(windows[i]->flags & WF_MINIMIZED)) {
            wm_focus(windows[i]);
            break;
        }
    }
    wm_invalidate_all();
}

int wm_current_desktop(void) {
    return current_desktop;
}

void wm_move_to_desktop(wm_window_t *w, int idx) {
    if (!w || idx < 0 || idx >= WM_MAX_DESKTOPS || idx == current_desktop) return;
    
    /* Remove from current desktop */
    for (int i = 0; i < win_count; i++) {
        if (windows[i] == w) {
            for (int j = i; j < win_count - 1; j++) {
                windows[j] = windows[j + 1];
            }
            win_count--;
            break;
        }
    }
    
    /* Add to target desktop */
    if (desktop_win_count[idx] < WM_MAX_WINDOWS) {
        desktop_windows[idx][desktop_win_count[idx]++] = w;
    }
}

void wm_tile_all(void) {
    int vis[WM_MAX_WINDOWS];
    int count = 0;
    for (int i = 0; i < win_count; i++) {
        if (windows[i]->visible && !(windows[i]->flags & WF_MINIMIZED)) {
            vis[count++] = i;
        }
    }
    if (count == 0) return;
    int work_h = screen_h - 40;
    if (count == 1) {
        wm_window_t *w = windows[vis[0]];
        w->frame.x = 90;
        w->frame.y = 50;
        w->frame.w = screen_w - 180;
        w->frame.h = work_h - 60;
        sync_surface(w);
        wm_invalidate(w);
    } else if (count == 2) {
        wm_window_t *w1 = windows[vis[0]];
        wm_window_t *w2 = windows[vis[1]];
        int half_w = (screen_w - 130) / 2;
        w1->frame.x = 100;
        w1->frame.y = 50;
        w1->frame.w = half_w;
        w1->frame.h = work_h - 60;
        w2->frame.x = 100 + half_w + 14;
        w2->frame.y = 50;
        w2->frame.w = half_w;
        w2->frame.h = work_h - 60;
        sync_surface(w1);
        sync_surface(w2);
        wm_invalidate(w1);
        wm_invalidate(w2);
    } else {
        int half_w = (screen_w - 130) / 2;
        int half_h = (work_h - 70) / 2;
        for (int i = 0; i < count && i < 4; i++) {
            wm_window_t *w = windows[vis[i]];
            w->frame.x = (i % 2 == 0) ? 100 : (100 + half_w + 14);
            w->frame.y = (i < 2) ? 50 : (half_h + 60);
            w->frame.w = half_w;
            w->frame.h = half_h;
            sync_surface(w);
            wm_invalidate(w);
        }
    }
}
