#include "term.h"
#include "theme.h"
#include "text.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "../kernel/process.h"
#include "../kernel/timer.h"
#include "../drivers/keyboard.h"

/* Embedded user ELFs, produced by the Makefile's ld -r -b binary embed. */
extern const uint8_t _binary_build_user_sh_elf_start[];
extern const uint8_t _binary_build_user_sh_elf_end[];

#define TERM_PAD_X 10
#define TERM_PAD_Y 8
#define CURSOR_BLINK_MS 530

static term_window_t *term_of(wm_window_t *w) { return (term_window_t *)w->user; }

static void term_paint(wm_window_t *w, surface_t *s, const rect_t *client) {
    term_window_t *t = term_of(w);
    if (!t || !t->vty) {
        surface_fill_rect(s, client, TH_TERM_BG);
        return;
    }
    vtty_t *v = t->vty;
    const baked_font_t *f = font_mono();
    int cw = 8, ch = 15;
    int cols = (client->w - TERM_PAD_X * 2) / cw;
    int rows = (client->h - TERM_PAD_Y * 2) / ch;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    surface_fill_rect(s, client, TH_TERM_BG);

    /* Grid rendering. Rows above the visible window come from scrollback, so
     * a long command's output can be paged with the mouse wheel. */
    int first = t->scroll_top;
    for (int r = 0; r < rows; r++) {
        int gy = first + r;
        int py = client->y + TERM_PAD_Y + r * ch;
        if (gy >= VTTY_ROWS) break;
        vtty_cell_t *cells = vtty_row(v, gy);
        for (int c = 0; c < cols; c++) {
            int gx = c;
            if (gx >= VTTY_COLS) break;
            vtty_cell_t *cell = &cells[gx];
            int px = client->x + TERM_PAD_X + c * cw;
            bool cursor_here = (gx == v->cx && gy == v->cy &&
                                t->cursor_on && v->cursor_visible);
            color_t fg = cell->fg == VGA_BRIGHT_BLACK ? TH_TEXT_DIM
                                                      : vga_color_rgb(cell->fg, true);
            if (cell->attr & VTTY_ATTR_DIM) fg = TH_TEXT_DIM;
            if (cell->attr & VTTY_ATTR_BOLD) fg = TH_TEXT_BRIGHT;
            if (cursor_here) {
                rect_t bgc = { px, py, cw, ch };
                surface_fill_rect(s, &bgc, TH_TERM_CURSOR);
                text_draw(s, f, px, py, " ", RGB(0x0A, 0x0D, 0x14));
            } else if (cell->ch != ' ') {
                /* One codepoint per cell, and the cell owns the position, so
                 * draw the glyph directly instead of building a string. That
                 * also lets a block element resolve to the block face while the
                 * rest of the line stays in the mono face. */
                text_draw_cp(s, f, px, py, cell->ch, fg);
                if (cell->attr & VTTY_ATTR_UNDERLINE) {
                    rect_t ul = { px, py + ch - 2, cw, 1 };
                    surface_fill_rect(s, &ul, fg);
                }
            }
        }
    }

    /* A scroll position indicator when the view is not at the live bottom. */
    if (t->scroll_top > 0) {
        rect_t bar = { client->x + client->w - 6, client->y + 4, 3, client->h - 8 };
        surface_rounded_fill(s, &bar, 2, TH_ACCENT_SOFT);
    }
}

static void term_update_cursor(term_window_t *t) {
    /* Blink only while the shell is waiting for input, which we approximate by
     * "the vty has no pending output this frame". */
    uint32_t now = timer_get_ms();
    if (now - t->blink_tick >= CURSOR_BLINK_MS) {
        t->blink_tick = now;
        t->cursor_on = !t->cursor_on;
    }
}

static bool term_event(wm_window_t *w, const gui_event_t *e) {
    term_window_t *t = term_of(w);
    if (!t || !t->vty) return false;

    if (e->type == EV_MOUSE_SCROLL) {
        /* Page the scrollback. Down scrolls toward the live output. */
        int rows = (w->client.h - TERM_PAD_Y * 2) / 15;
        if (e->scroll > 0) {
            int maxs = VTTY_ROWS - rows;
            if (maxs < 0) maxs = 0;
            t->scroll_top -= 3;
            if (t->scroll_top < 0) t->scroll_top = 0;
        } else {
            int maxs = VTTY_ROWS - rows;
            if (maxs < 0) maxs = 0;
            t->scroll_top += 3;
            if (t->scroll_top > maxs) t->scroll_top = maxs;
        }
        w->dirty = true;
        return true;
    }

    if (e->type == EV_MOUSE_DOWN) {
        /* Any interaction returns the view to the live output. */
        t->scroll_top = 0;
        w->dirty = true;
    }

    if (e->type != EV_KEY_DOWN) return false;

    /* Typing anywhere in the window returns to the live output too. */
    t->scroll_top = 0;
    t->cursor_on = true;
    t->blink_tick = timer_get_ms();

    /* Ctrl+C interrupts, matching the serial console. */
    if (e->modifiers & KMOD_CTRL) {
        switch (e->keycode) {
            case KEY_C: vtty_push_char(t->vty, 3); w->dirty = true; return true;
            case KEY_D: vtty_push_char(t->vty, 4); w->dirty = true; return true;
            case KEY_L: vtty_push_char(t->vty, 12); w->dirty = true; return true;
            case KEY_U: vtty_push_char(t->vty, 21); w->dirty = true; return true;
            case KEY_A: vtty_push_char(t->vty, 1); w->dirty = true; return true;
            case KEY_E: vtty_push_char(t->vty, 5); w->dirty = true; return true;
            default: break;
        }
    }

    /* Navigation keys arrive with no ASCII from the driver, so translate the
     * ones the shell understands. */
    switch (e->keycode) {
        case GUIKEY_ENTER:     vtty_push_char(t->vty, '\n'); w->dirty = true; return true;
        case GUIKEY_BACKSPACE: vtty_push_char(t->vty, 8);   w->dirty = true; return true;
        case GUIKEY_TAB:       vtty_push_char(t->vty, '\t'); w->dirty = true; return true;
        case GUIKEY_ESCAPE:    vtty_push_char(t->vty, 27);  w->dirty = true; return true;
        case GUIKEY_UP:        vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[A"); w->dirty = true; return true;
        case GUIKEY_DOWN:      vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[B"); w->dirty = true; return true;
        case GUIKEY_LEFT:      vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[D"); w->dirty = true; return true;
        case GUIKEY_RIGHT:     vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[C"); w->dirty = true; return true;
        case GUIKEY_HOME:      vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[H"); w->dirty = true; return true;
        case GUIKEY_END:       vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[F"); w->dirty = true; return true;
        case GUIKEY_PGUP:      vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[5~"); w->dirty = true; return true;
        case GUIKEY_PGDN:      vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[6~"); w->dirty = true; return true;
        case GUIKEY_DELETE:    vtty_push_char(t->vty, 0x1B); vtty_push_str(t->vty, "[3~"); w->dirty = true; return true;
        default: break;
    }

    if (e->ascii) {
        vtty_push_char(t->vty, e->ascii);
        w->dirty = true;
        return true;
    }
    return false;
}

void term_attach(wm_window_t *w, vtty_t *v) {
    term_window_t *t = term_of(w);
    if (!t) return;
    t->vty = v;
    t->last_revision = 0;
    t->scroll_top = 0;
    if (v) { v->attached = true; v->cursor_visible = true; }
    wm_invalidate(w);
}

vtty_t *term_vty_of(wm_window_t *w) {
    term_window_t *t = term_of(w);
    return t ? t->vty : NULL;
}

void term_notify_dirty(wm_window_t *w) { wm_invalidate(w); }

/* Called once per desktop frame: repaint a terminal only when its vty changed,
 * so an idle shell costs nothing. */
static void term_service(void) {
    for (int i = 0; i < wm_window_count(); i++) {
        wm_window_t *w = wm_window_at(i);
        if (!w || !w->user) continue;
        if (strcmp(w->title, "Terminal") != 0 && strcmp(w->title, "MyOS Terminal") != 0)
            continue;
        term_window_t *t = term_of(w);
        if (!t || !t->vty) continue;
        if (t->vty->revision != t->last_revision) {
            t->last_revision = t->vty->revision;
            w->dirty = true;
        } else {
            term_update_cursor(t);
        }
    }
}

wm_window_t *term_open(const char *title) {
    wm_window_t *w = wm_create(title, 120, 90, 720, 460);
    if (!w) return NULL;
    term_window_t *t = kzalloc(sizeof(term_window_t));
    if (!t) { wm_destroy(w); return NULL; }
    t->cursor_on = true;
    t->blink_tick = timer_get_ms();
    w->user = t;
    w->paint = term_paint;
    w->event = term_event;
    term_attach(w, vtty_alloc(title ? "term" : "term"));
    return w;
}

wm_window_t *term_open_shell(void) {
    wm_window_t *w = term_open("MyOS Terminal");
    if (!w) return NULL;
    vtty_t *v = term_vty_of(w);
    if (!v) { wm_destroy(w); return NULL; }

    /* A greeting so an empty window is never mistaken for a hang. */
    vtty_puts(v, "\x1b[1;36mMyOS\x1b[0m graphical terminal\r\n");
    vtty_puts(v, "This window is a real VT100 terminal. The shell below is a\r\n");
    vtty_puts(v, "ring-3 process; try \x1b[1;33mhelp\x1b[0m, \x1b[1;33mls\x1b[0m, ");
    vtty_puts(v, "\x1b[1;33mps\x1b[0m, \x1b[1;33mfree\x1b[0m, \x1b[1;33mrun hello\x1b[0m.\r\n");
    vtty_puts(v, "Type \x1b[1;33mexit\x1b[0m and the window closes.\r\n\r\n");

    /* Spawn the real shell and bind it to this window's vty, so its output
     * lands here and its input comes from this window's keyboard. */
    process_t *p = process_create_user("sh", _binary_build_user_sh_elf_start,
                                       (uint64_t)(_binary_build_user_sh_elf_end -
                                                  _binary_build_user_sh_elf_start));
    if (p) console_bind_pid(p->pid, v);
    wm_invalidate(w);
    return w;
}

/* Hook invoked once per desktop frame; declared here so the desktop can drive
 * terminal repaints without term.c depending on desktop.c. */
void (*term_service_ptr)(void) = NULL;

void term_register_service(void) { term_service_ptr = term_service; }
