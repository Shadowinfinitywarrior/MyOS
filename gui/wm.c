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

/* Button slots, measured from the right edge of the title bar. */
#define BTN_W 30
#define BTN_H 16
static int btn_x(const wm_window_t *w, int slot) {
    /* slot 0 = close (rightmost), 1 = maximize, 2 = minimize */
    return w->frame.x + w->frame.w - WM_BORDER_W - BTN_W * (slot + 1) + (BTN_W - BTN_H) / 2;
}

static hit_t hit_test(const wm_window_t *w, int x, int y, wm_edge_t *edge_out) {
    if (edge_out) *edge_out = EDGE_NONE;
    if (!rect_contains_point(&w->frame, x, y)) return HIT_CLIENT;
    if (w->flags & WF_NO_DECOR) return HIT_CLIENT;

    /* The title bar stays hit-testable while maximized, otherwise the window
     * buttons and the double-click-to-restore gesture become unreachable the
     * moment the window is maximized - there is no other way out. */
    int tb_y = w->frame.y + WM_BORDER_W;
    if (y >= tb_y && y < tb_y + WM_TITLEBAR_H) {
        for (int slot = 0; slot < 3; slot++) {
            int bx = btn_x(w, slot);
            int by = tb_y + (WM_TITLEBAR_H - BTN_H) / 2;
            if (x >= bx && x < bx + BTN_H && y >= by && y < by + BTN_H)
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
        } else {
            wm_focus(w);
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

static void glyph_x(surface_t *s, int cx, int cy, color_t col) {
    for (int i = -3; i <= 3; i++) {
        surface_pixel(s, cx + i, cy + i, col);
        surface_pixel(s, cx - i, cy + i, col);
    }
}

static void glyph_min(surface_t *s, int cx, int cy, color_t col) {
    for (int i = -4; i <= 4; i++) surface_pixel(s, cx + i, cy, col);
}

static void glyph_max(surface_t *s, int cx, int cy, color_t col, bool maximized) {
    for (int i = -4; i <= 4; i++) {
        surface_pixel(s, cx + i, cy - 4, col);
        surface_pixel(s, cx + i, cy + 4, col);
        surface_pixel(s, cx - 4, cy + i, col);
        surface_pixel(s, cx + 4, cy + i, col);
    }
    if (maximized) {
        for (int i = -2; i <= 2; i++) {
            surface_pixel(s, cx + i, cy - 1, col);
            surface_pixel(s, cx + i, cy - 7, col);
            surface_pixel(s, cx - 6, cy - 1 + i, col);
            surface_pixel(s, cx - 3, cy - 1 + i, col);
        }
    }
}

static void draw_decorations(wm_window_t *w) {
    surface_t *s = w->surf;
    if (!s) return;

    color_t border = w->focused ? TH_BORDER_FOCUS : TH_BORDER;
    color_t bar    = w->focused ? TH_TITLE_FOCUS : TH_TITLE;
    color_t text   = w->focused ? TH_TITLE_TEXT : TH_TITLE_TEXT_DIM;

    /* Frame border. */
    rect_t r = { 0, 0, w->frame.w, w->frame.h };
    surface_rect_outline(s, &r, border, WM_BORDER_W);
    if (w->frame.w > 2 && w->frame.h > 2) {
        rect_t inner = { 1, 1, w->frame.w - 2, w->frame.h - 2 };
        surface_rect_outline(s, &inner, border, 1);
    }

    if (w->flags & WF_NO_DECOR) return;

    /* Title bar. */
    rect_t tb = { WM_BORDER_W, WM_BORDER_W, w->frame.w - WM_BORDER_W * 2, WM_TITLEBAR_H };
    surface_fill_rect(s, &tb, bar);
    /* Accent stripe on the left of the title bar, like a modern title bar. */
    rect_t stripe = { WM_BORDER_W, WM_BORDER_W, 3, WM_TITLEBAR_H };
    surface_fill_rect(s, &stripe, w->focused ? TH_ACCENT : TH_BORDER);

    int ty = tb.y + (tb.h - 15) / 2;
    text_draw_n(s, font_bold(), tb.x + 12, ty, w->title,
                w->frame.w - 120, text);

    /* Buttons, right to left. */
    int by = tb.y + (tb.h - BTN_H) / 2;
    int x_close = w->frame.w - WM_BORDER_W - BTN_W + (BTN_W - BTN_H) / 2;
    int x_max   = x_close - BTN_W;
    int x_min   = x_max - BTN_W;

    if (w->focused) {
        rect_t cb = { x_close, by, BTN_H, BTN_H };
        surface_rounded_fill(s, &cb, 4, TH_BTN_CLOSE);
        glyph_x(s, x_close + BTN_H / 2, by + BTN_H / 2, RGB(0xFF, 0xFF, 0xFF));
    } else {
        glyph_x(s, x_close + BTN_H / 2, by + BTN_H / 2, TH_TITLE_TEXT_DIM);
    }
    glyph_min(s, x_max + BTN_H / 2, by + BTN_H / 2, TH_TITLE_TEXT);
    glyph_max(s, x_min + BTN_H / 2, by + BTN_H / 2, TH_TITLE_TEXT,
              (w->flags & WF_MAXIMIZED) != 0);
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

        /* Drop shadow on the back buffer, beneath the window. */
        uint32_t *bb = fb_get_backbuffer();
        if (bb && !(w->flags & WF_MAXIMIZED)) {
            int stride = fb_get_stride();
            rect_t outer = { w->frame.x - WM_SHADOW_PAD + 4, w->frame.y - WM_SHADOW_PAD + 8,
                             w->frame.w + WM_SHADOW_PAD * 2, w->frame.h + WM_SHADOW_PAD * 2 };
            rect_t screen = { 0, 0, screen_w, screen_h };
            rect_t vis;
            if (rect_intersect(&outer, &screen, &vis)) {
                for (int y = vis.y; y < vis.y + vis.h; y++) {
                    uint32_t *row = bb + (size_t)y * (uint32_t)stride;
                    for (int x = vis.x; x < vis.x + vis.w; x++) {
                        /* Darken only in the ring between the frame and the
                         * outer edge, so the shadow fades outward. */
                        bool inside_frame = x >= w->frame.x && x < w->frame.x + w->frame.w &&
                                            y >= w->frame.y && y < w->frame.y + w->frame.h;
                        if (inside_frame) continue;
                        int dx = x < w->frame.x ? w->frame.x - x
                               : (x >= w->frame.x + w->frame.w ? x - (w->frame.x + w->frame.w - 1) : 0);
                        int dy = y < w->frame.y ? w->frame.y - y
                               : (y >= w->frame.y + w->frame.h ? y - (w->frame.y + w->frame.h - 1) : 0);
                        int d = dx > dy ? dx : dy;
                        if (d > WM_SHADOW_PAD) continue;
                        uint32_t c = row[x];
                        uint32_t f = (uint32_t)(d * 24) / (uint32_t)WM_SHADOW_PAD;
                        uint32_t r = ((c >> 16) & 0xFF) * (64 - f) / 64;
                        uint32_t g = ((c >> 8) & 0xFF) * (64 - f) / 64;
                        uint32_t b = (c & 0xFF) * (64 - f) / 64;
                        row[x] = (r << 16) | (g << 8) | b;
                    }
                }
                fb_add_damage(vis.x, vis.y, vis.w, vis.h);
            }
        }

        if (w->surf) surface_present(w->surf, w->frame.x, w->frame.y, NULL);
    }
}
