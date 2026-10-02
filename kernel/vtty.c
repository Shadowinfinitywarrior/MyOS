#include "vtty.h"
#include "process.h"
#include "../lib/string.h"
#include "../include/system.h"

/* Standard VGA attribute byte for a cell, used only by vtty_render_vga. */
static const uint8_t vga_palette[VGA_COLOR_COUNT] = {
    0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07,
    0x08, 0x0C, 0x0A, 0x0E, 0x09, 0x0D, 0x0B, 0x0F
};

static vtty_t vttys[VTTY_MAX];
static vtty_t *boot_console;
static volatile int vtty_lock;

static inline void vtty_enter(void) {
    while (__atomic_test_and_set(&vtty_lock, __ATOMIC_ACQUIRE)) { }
}
static inline void vtty_exit(void) {
    __atomic_clear(&vtty_lock, __ATOMIC_RELEASE);
}

void vtty_init(void) {
    for (int i = 0; i < VTTY_MAX; i++) vttys[i].used = false;
    boot_console = vtty_alloc("console");
}

vtty_t *vtty_alloc(const char *name) {
    for (int i = 0; i < VTTY_MAX; i++) {
        if (vttys[i].used) continue;
        vtty_t *v = &vttys[i];
        memset(v, 0, sizeof(*v));
        v->used = true;
        v->fg = VGA_WHITE;
        v->bg = VGA_BLACK;
        v->cursor_visible = true;
        v->scroll_top = 0;
        v->scroll_bottom = VTTY_ROWS - 1;
        if (name) {
            int n = (int)strlen(name);
            if (n > 15) n = 15;
            for (int k = 0; k < n; k++) v->name[k] = name[k];
            v->name[n] = 0;
        }
        for (int y = 0; y < VTTY_ROWS; y++)
            for (int x = 0; x < VTTY_COLS; x++) {
                v->cells[y][x].ch = ' ';
                v->cells[y][x].fg = VGA_WHITE;
                v->cells[y][x].bg = VGA_BLACK;
                v->cells[y][x].attr = 0;
            }
        v->revision = 1;
        return v;
    }
    return NULL;
}

vtty_t *vtty_get(int id) {
    if (id < 0 || id >= VTTY_MAX || !vttys[id].used) return NULL;
    return &vttys[id];
}

int vtty_count(void) {
    int n = 0;
    for (int i = 0; i < VTTY_MAX; i++) if (vttys[i].used) n++;
    return n;
}

void vtty_free(vtty_t *v) {
    if (!v) return;
    v->used = false;
    if (boot_console == v) boot_console = NULL;
}

vtty_cell_t *vtty_row(vtty_t *v, int y) {
    if (!v || y < 0 || y >= VTTY_ROWS) return NULL;
    return v->cells[y];
}

int vtty_cols(const vtty_t *v) { return v ? VTTY_COLS : 0; }
int vtty_rows(const vtty_t *v) { return v ? VTTY_ROWS : 0; }

void vtty_set_color(vtty_t *v, uint8_t fg, uint8_t bg) {
    if (!v) return;
    v->fg = fg;
    v->bg = bg;
}

void vtty_set_attr(vtty_t *v, uint8_t attr) {
    if (!v) return;
    v->attr = attr;
}

void vtty_move(vtty_t *v, int x, int y) {
    if (!v) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= VTTY_COLS) x = VTTY_COLS - 1;
    if (y >= VTTY_ROWS) y = VTTY_ROWS - 1;
    v->cx = x;
    v->cy = y;
}

void vtty_clear(vtty_t *v) {
    if (!v) return;
    for (int y = 0; y < VTTY_ROWS; y++)
        for (int x = 0; x < VTTY_COLS; x++) {
            v->cells[y][x].ch = ' ';
            v->cells[y][x].fg = v->fg;
            v->cells[y][x].bg = v->bg;
            v->cells[y][x].attr = v->attr;
        }
    v->cx = 0;
    v->cy = 0;
    v->revision++;
}

void vtty_scroll(vtty_t *v) {
    if (!v) return;
    int top = v->scroll_top, bot = v->scroll_bottom;
    for (int y = top; y < bot; y++) {
        for (int x = 0; x < VTTY_COLS; x++) v->cells[y][x] = v->cells[y + 1][x];
    }
    for (int x = 0; x < VTTY_COLS; x++) {
        v->cells[bot][x].ch = ' ';
        v->cells[bot][x].fg = v->fg;
        v->cells[bot][x].bg = v->bg;
        v->cells[bot][x].attr = v->attr;
    }
    v->cy = bot;
    v->cx = 0;
    v->revision++;
}

/* ---- escape sequence state --------------------------------------------- */

typedef struct {
    const char *seq;      /* in-progress CSI sequence, "" when idle */
    int         sgr[8];   /* parsed SGR parameters  */
    int         sgr_n;
} esc_state_t;

static esc_state_t esc;

static void put_cell(vtty_t *v, int x, int y, uint32_t c) {
    v->cells[y][x].ch = c;
    v->cells[y][x].fg = v->fg;
    v->cells[y][x].bg = v->bg;
    v->cells[y][x].attr = v->attr;
}

static void sgr_reset(vtty_t *v) {
    v->fg = VGA_WHITE;
    v->bg = VGA_BLACK;
    v->attr = 0;
}

static void sgr_apply(vtty_t *v, int n, const int *p) {
    for (int i = 0; i < n; i++) {
        int code = p[i];
        if (code == 0) { sgr_reset(v); }
        else if (code == 1) v->attr |= VTTY_ATTR_BOLD;
        else if (code == 2) v->attr |= VTTY_ATTR_DIM;
        else if (code == 4) v->attr |= VTTY_ATTR_UNDERLINE;
        else if (code == 7) v->attr |= VTTY_ATTR_REVERSE;
        else if (code == 22) v->attr &= (uint8_t)~(VTTY_ATTR_BOLD | VTTY_ATTR_DIM);
        else if (code == 24) v->attr &= (uint8_t)~VTTY_ATTR_UNDERLINE;
        else if (code == 27) v->attr &= (uint8_t)~VTTY_ATTR_REVERSE;
        else if (code >= 30 && code <= 37) v->fg = (uint8_t)(code - 30);
        else if (code == 39) v->fg = VGA_WHITE;
        else if (code >= 40 && code <= 47) v->bg = (uint8_t)(code - 40);
        else if (code == 49) v->bg = VGA_BLACK;
        else if (code >= 90 && code <= 97) v->fg = (uint8_t)(code - 90 + 8);
        else if (code >= 100 && code <= 107) v->bg = (uint8_t)(code - 100 + 8);
    }
}

/* Handle one byte of a CSI sequence. Returns true if the sequence is done
 * (and has been dispatched). */
static bool esc_dispatch(vtty_t *v, char c) {
    if (c >= '0' && c <= '9') {
        if (esc.sgr_n < 8) esc.sgr[esc.sgr_n++] = esc.sgr[0] * 10 + (c - '0');
        return false;
    }
    if (c == ';') {
        if (esc.sgr_n < 8) {
            esc.sgr[esc.sgr_n++] = esc.sgr[0];
            esc.sgr[esc.sgr_n] = 0;
        }
        return false;
    }
    /* Final byte. */
    int n = esc.sgr_n ? esc.sgr_n : 1;
    if (esc.sgr_n) { esc.sgr[esc.sgr_n] = 0; }
    else { esc.sgr[0] = 0; }

    switch (c) {
        case 'm': sgr_apply(v, n, esc.sgr); break;
        case 'A': vtty_move(v, v->cx, v->cy - (n ? esc.sgr[0] : 1)); break;
        case 'B': vtty_move(v, v->cx, v->cy + (n ? esc.sgr[0] : 1)); break;
        case 'C': vtty_move(v, v->cx + (n ? esc.sgr[0] : 1), v->cy); break;
        case 'D': vtty_move(v, v->cx - (n ? esc.sgr[0] : 1), v->cy); break;
        case 'H': case 'f':
            vtty_move(v, (n > 0 && esc.sgr[0] > 0 ? esc.sgr[0] - 1 : 0),
                          (n > 1 && esc.sgr[1] > 0 ? esc.sgr[1] - 1 : 0));
            break;
        case 'J': {
            int mode = n ? esc.sgr[0] : 0;
            if (mode == 2 || mode == 3) vtty_clear(v);
            else if (mode == 0) {
                for (int x = v->cx; x < VTTY_COLS; x++) put_cell(v, x, v->cy, ' ');
                for (int y = v->cy + 1; y < VTTY_ROWS; y++)
                    for (int x = 0; x < VTTY_COLS; x++) put_cell(v, x, y, ' ');
            } else {
                for (int y = 0; y < v->cy; y++)
                    for (int x = 0; x < VTTY_COLS; x++) put_cell(v, x, y, ' ');
                for (int x = 0; x <= v->cx && v->cy < VTTY_ROWS; x++) put_cell(v, x, v->cy, ' ');
            }
            v->revision++;
            break;
        }
        case 'K': {
            int mode = n ? esc.sgr[0] : 0;
            if (mode == 0) for (int x = v->cx; x < VTTY_COLS; x++) put_cell(v, x, v->cy, ' ');
            else if (mode == 1) for (int x = 0; x <= v->cx; x++) put_cell(v, x, v->cy, ' ');
            else for (int x = 0; x < VTTY_COLS; x++) put_cell(v, x, v->cy, ' ');
            v->revision++;
            break;
        }
        default: break;
    }
    return true;
}

/* Feed one byte through the vty's incremental UTF-8 decoder.
 *
 * Returns true when `cp` holds a complete character to be drawn, false when the
 * byte was only part of a sequence. Bytes that cannot start or continue a valid
 * sequence are reported as U+FFFD so malformed input still advances the cursor
 * instead of silently swallowing text. The decoder lives in the vty rather than
 * on the stack because the write syscall hands over at most 256 bytes per call,
 * so a three-byte character can straddle two calls. */
static bool utf8_feed(vtty_t *v, uint8_t b, uint32_t *cp) {
    if (v->u8_left > 0) {
        if ((b & 0xC0) == 0x80) {
            v->u8_pending = (v->u8_pending << 6) | (uint32_t)(b & 0x3F);
            if (--v->u8_left == 0) { *cp = v->u8_pending; return true; }
            return false;
        }
        /* Truncated sequence: report it as U+FFFD and drop the offending byte.
         * Re-feeding it here would overwrite the replacement character, and the
         * only way to keep both is a pushback slot for a stream that is already
         * malformed. Losing one byte of garbage is the better trade. */
        v->u8_left = 0;
        v->u8_pending = 0;
        *cp = 0xFFFD;
        return true;
    }
    if (b < 0x80) { *cp = b; return true; }
    if (b >= 0xC2 && b <= 0xDF) { v->u8_pending = b & 0x1F; v->u8_left = 1; return false; }
    if (b >= 0xE0 && b <= 0xEF) { v->u8_pending = b & 0x0F; v->u8_left = 2; return false; }
    if (b >= 0xF0 && b <= 0xF4) { v->u8_pending = b & 0x07; v->u8_left = 3; return false; }
    *cp = 0xFFFD;   /* continuation byte out of nowhere, or an overlong/invalid lead */
    return true;
}

void vtty_putc(vtty_t *v, char c) {
    if (!v || !v->used) return;

    /* Escape state machine. `esc.seq` is non-NULL while a CSI is in progress.
     * Escape sequences are ASCII by definition, so this runs on the raw byte
     * ahead of the UTF-8 decoder and the two never interfere. */
    if (esc.seq) {
        if (c == '[') { esc.sgr[0] = 0; esc.sgr_n = 0; return; }
        if (esc_dispatch(v, c)) esc.seq = NULL;
        return;
    }
    if (c == 0x1B) { esc.seq = (const char *)""; return; }

    /* Control characters are single-byte and must not disturb a pending
     * multi-byte sequence, so handle them before the decoder. */
    if ((uint8_t)c < 0x20 || (uint8_t)c == 0x7F) {
        vtty_enter();
        switch (c) {
            case '\r': v->cx = 0; break;
            case '\n':
                /* Cooked-mode ONLCR: a line feed also returns to column 0.
                 * Ring-3 programs emit a bare '\n' (libc puts() appends only
                 * '\n', and putchar() goes through SYS_PUTCHAR), so without this
                 * every line after the first is indented by the length of the one
                 * before it and multi-line output renders as a staircase that
                 * wraps mid-word. Kernel-side output already sends explicit
                 * "\r\n", so the extra carriage return is a no-op there. */
                v->cx = 0;
                v->cy++;
                if (v->cy > v->scroll_bottom) vtty_scroll(v);
                break;
            case '\b':
                if (v->cx > 0) v->cx--;
                else if (v->cy > v->scroll_top) { v->cy--; v->cx = VTTY_COLS - 1; }
                break;
            case '\t':
                v->cx = (v->cx + 8) & ~7;
                if (v->cx >= VTTY_COLS) { v->cx = 0; v->cy++; }
                break;
            case '\a':
                break;
            default:
                break;
        }
        if (v->cy > v->scroll_bottom) vtty_scroll(v);
        v->revision++;
        vtty_exit();
        return;
    }

    {
        uint32_t cp;
        vtty_enter();
        bool have = utf8_feed(v, (uint8_t)c, &cp);
        if (have) {
            put_cell(v, v->cx, v->cy, cp);
            v->cx++;
            if (v->cx >= VTTY_COLS) { v->cx = 0; v->cy++; }
        }
        if (v->cy > v->scroll_bottom) vtty_scroll(v);
        v->revision++;
        vtty_exit();
    }
}

void vtty_write(vtty_t *v, const char *s, int len) {
    for (int i = 0; i < len && s[i]; i++) vtty_putc(v, s[i]);
}

void vtty_puts(vtty_t *v, const char *s) {
    if (!s) return;
    while (*s) vtty_putc(v, *s++);
}

/* ---- input ring -------------------------------------------------------- */

void vtty_push_char(vtty_t *v, char c) {
    if (!v) return;
    vtty_enter();
    int next = (v->in_head + 1) % VTTY_INPUT_CAP;
    if (next != v->in_tail) {
        v->input[v->in_head] = c;
        v->in_head = next;
    }
    vtty_exit();
}

void vtty_push_str(vtty_t *v, const char *s) {
    if (!s) return;
    while (*s) vtty_push_char(v, *s++);
}

bool vtty_pop_char(vtty_t *v, char *out) {
    if (!v) return false;
    bool got = false;
    vtty_enter();
    if (v->in_tail != v->in_head) {
        if (out) *out = v->input[v->in_tail];
        v->in_tail = (v->in_tail + 1) % VTTY_INPUT_CAP;
        got = true;
    }
    vtty_exit();
    return got;
}

bool vtty_input_empty(const vtty_t *v) {
    return !v || v->in_head == v->in_tail;
}

void vtty_flush_input(vtty_t *v) {
    if (!v) return;
    v->in_head = v->in_tail = 0;
}

/* ---- console binding --------------------------------------------------- */

void console_set_boot(vtty_t *v) { boot_console = v; }
vtty_t *console_boot(void) { return boot_console; }

void console_bind_current(vtty_t *v) {
    process_t *p = process_get_current();
    if (p && v) p->vtty = v;
}

void console_bind_pid(int pid, vtty_t *v) {
    process_t *p = process_get_by_pid(pid);
    if (p && v) p->vtty = v;
}

vtty_t *console_for_current(void) {
    process_t *p = process_get_current();
    if (p && p->vtty) return p->vtty;
    return boot_console;
}

/* ---- VGA text rendering (serial/boot console path) --------------------- */

void vtty_render_vga(vtty_t *v) {
    if (!v) return;
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    for (int y = 0; y < VTTY_ROWS; y++) {
        for (int x = 0; x < VTTY_COLS; x++) {
            vtty_cell_t *c = &v->cells[y][x];
            uint8_t fg = c->fg, bg = c->bg;
            if (c->attr & VTTY_ATTR_REVERSE) { uint8_t t = fg; fg = bg; bg = t; }
            if (c->attr & VTTY_ATTR_BOLD) fg |= 8;
            /* The VGA text buffer holds one byte per cell, so anything outside
             * Latin-1 has to collapse. Substitute '?' rather than truncating:
             * the low byte of a block-element codepoint is a control-range value
             * that would print as garbage or move the cursor. */
            uint16_t ch = (c->ch >= 0x20 && c->ch <= 0xFF) ? (uint16_t)c->ch : '?';
            vga[y * VTTY_COLS + x] = ch |
                                     ((uint16_t)vga_palette[bg & 15] << 4) |
                                     ((uint16_t)vga_palette[fg & 15] << 8);
        }
    }
}
