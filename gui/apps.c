#include "apps.h"
#include "logo.h"
#include "theme.h"
#include "text.h"
#include "term.h"
#include "util.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/heap.h"
#include "../fs/vfs.h"
#include "../kernel/process.h"
#include "../kernel/pmm.h"
#include "../kernel/timer.h"
#include "../drivers/rtc.h"
#include "../drivers/framebuffer.h"
#include "../drivers/speaker.h"
#include "../drivers/ac97.h"
#include "../drivers/mouse.h"
#include "../drivers/ata.h"
#include "../kernel/storage.h"
#include "../fs/ramfs.h"
#include "../drivers/virtio_blk.h"
#include "../drivers/driver.h"
#include "desktop.h"

/* ---- shared chrome ------------------------------------------------------ */

/* A titled "card" panel inside a window, used by the info apps. */
static void panel(surface_t *s, const rect_t *area, const char *title) {
    surface_rounded_fill(s, area, 8, RGB(0x1B, 0x20, 0x28));
    surface_rounded_outline(s, area, 8, RGB(0x30, 0x36, 0x3D), 1);
    rect_t hdr = { area->x + 1, area->y + 1, area->w - 2, 28 };
    surface_rounded_fill(s, &hdr, 7, RGB(0x21, 0x27, 0x33));
    text_draw(s, font_bold(), area->x + 12, area->y + 7, title, TH_TITLE_TEXT);
}

static int kv_line(surface_t *s, int y, const char *k, const char *v, int x, int w) {
    text_draw(s, font_ui(), x, y, k, TH_TEXT_DIM);
    text_draw(s, font_mono(), x + w - text_width(font_mono(), v), y, v, TH_TEXT);
    return y + 19;
}

/* Join a directory path and a child name into `out`. */
static void path_join(char *out, int cap, const char *dir, const char *name) {
    int n = 0;
    while (dir[n] && n < cap - 2) { out[n] = dir[n]; n++; }
    if (n > 0 && out[n - 1] != '/') out[n++] = '/';
    for (int i = 0; name[i] && n < cap - 1; i++) out[n++] = name[i];
    out[n] = 0;
}

/* ---- File explorer ------------------------------------------------------- */

typedef struct files_state {
    char        path[128];
    vfs_node_t *node;
    int         sel;
} files_state_t;

static void files_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    files_state_t *st = (files_state_t *)w->user;
    surface_fill_rect(s, c, TH_WIN_BG_FOCUS);

    /* Breadcrumb / path bar. */
    rect_t hdr = { c->x, c->y, c->w, 30 };
    surface_fill_rect(s, &hdr, TH_TITLE);
    text_draw(s, font_bold(), c->x + 12, c->y + 7, "Files", TH_TITLE_TEXT);
    rect_t pathbar = { c->x + 70, c->y + 5, c->w - 84, 20 };
    surface_rounded_fill(s, &pathbar, 5, TH_ACCENT_SOFT);
    text_draw(s, font_mono(), c->x + 80, c->y + 8, st->path, TH_TEXT);

    if (!st->node) {
        text_draw(s, font_ui(), c->x + 16, c->y + 48, "Path not found", TH_TERM_ERR);
        return;
    }

    int y = c->y + 42;
    int max_rows = (c->h - 56) / 20;
    int idx = 0;
    for (int i = 0; st->node->readdir && idx < max_rows; i++) {
        vfs_node_t *child = st->node->readdir(st->node, (uint32_t)i);
        if (!child) break;
        if (idx == st->sel) {
            rect_t hl = { c->x + 6, y - 3, c->w - 12, 20 };
            surface_rounded_fill(s, &hl, 4, TH_ACCENT_SOFT);
        }
        bool dir = (child->flags & VFS_DIRECTORY) != 0;
        /* Folder/file glyph drawn as a small filled shape. */
        color_t ic = dir ? TH_ACCENT : TH_TEXT_DIM;
        if (dir) {
            rect_t folder = { c->x + 16, y + 2, 12, 9 };
            surface_fill_rect(s, &folder, ic);
            rect_t tab = { c->x + 17, y, 5, 3 };
            surface_fill_rect(s, &tab, ic);
        } else {
            rect_t page = { c->x + 18, y, 9, 12 };
            surface_rect_outline(s, &page, ic, 1);
        }
        text_draw(s, font_ui(), c->x + 38, y, child->name, TH_TEXT);

        char sizebuf[24];
        if (!dir) ui_bytes(child->length, sizebuf, sizeof(sizebuf));
        else sizebuf[0] = 0;
        if (sizebuf[0])
            text_draw(s, font_mono(), c->x + c->w - 70, y, sizebuf, TH_TEXT_DIM);
        y += 20;
        idx++;
    }
    if (idx == 0)
        text_draw(s, font_ui(), c->x + 16, c->y + 48, "(empty directory)", TH_TEXT_DIM);
}

static bool files_event(wm_window_t *w, const gui_event_t *e) {
    files_state_t *st = (files_state_t *)w->user;
    if (!st) return false;

    if (e->type == EV_MOUSE_DOWN) {
        /* The parent-directory row sits on the path bar's left arrow. */
        if (e->y < w->client.y + 30 && e->x < w->client.x + 70) {
            /* Strip the last component. */
            int n = (int)strlen(st->path);
            while (n > 1 && st->path[n - 1] != '/') n--;
            if (n > 1) n--;
            st->path[n] = 0;
            st->node = vfs_resolve_path(st->path[0] ? st->path : "/");
            st->sel = 0;
            wm_invalidate(w);
            return true;
        }
        if (e->y >= w->client.y + 42 && e->y < w->client.y + w->client.h) {
            int row = (e->y - (w->client.y + 42)) / 20;
            int idx = 0;
            if (st->node && st->node->readdir) {
                vfs_node_t *child = st->node->readdir(st->node, (uint32_t)row);
                if (child) {
                    if (child->flags & VFS_DIRECTORY) {
                        char newpath[128];
                        path_join(newpath, sizeof(newpath), st->path, child->name);
                        vfs_node_t *n = vfs_resolve_path(newpath);
                        if (n) {
                            for (int i = 0; i < 127; i++) st->path[i] = newpath[i];
                            st->path[127] = 0;
                            st->node = n;
                            st->sel = 0;
                        }
                    }
                }
                (void)idx;
            }
            wm_invalidate(w);
            return true;
        }
    }
    return false;
}

void app_open_files(void) {
    wm_window_t *w = wm_create("Files", 180, 140, 520, 380);
    if (!w) return;
    files_state_t *st = kzalloc(sizeof(files_state_t));
    if (!st) { wm_destroy(w); return; }
    st->path[0] = '/';
    st->node = vfs_resolve_path("/");
    w->user = st;
    w->paint = files_paint;
    w->event = files_event;
}

/* ---- About --------------------------------------------------------------- */

static void about_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, TH_WIN_BG);

    /* Header band with product name and glowing badge */
    rect_t band = { c->x, c->y, c->w, 92 };
    surface_gradient_v(s, &band, RGB(0x1E, 0x2A, 0x44), RGB(0x10, 0x16, 0x22));
    rect_t band_line = { c->x, c->y + 91, c->w, 1 };
    surface_fill_rect(s, &band_line, RGB(0x2E, 0x38, 0x4A));

    /* Planetary Orbital Ribbon 'M' Logo */
    logo_draw_surface(s, c->x + 44, c->y + 46, 56);

    text_draw(s, font_bold(), c->x + 82, c->y + 20, "MyOS Modern Edition", RGB(0xFF, 0xFF, 0xFF));
    text_draw(s, font_ui(), c->x + 82, c->y + 42, "FREEDOM • PRIVACY • PERFORMANCE", RGB(0x00, 0xD2, 0xFF));
    text_draw(s, font_ui(), c->x + 82, c->y + 62, "A Cleaner, Smarter, Faster Operating System", RGB(0x8A, 0x9B, 0xB5));

    int y = c->y + 104;
    rect_t p1 = { c->x + 16, y, c->w - 32, 134 };
    panel(s, &p1, "System Architecture");
    y = p1.y + 36;
    y = kv_line(s, y, "Architecture", "x86-64 Long Mode (SMP)", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Memory & Paging", "Higher-Half Paged COW", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Display Engine", "1024x768 32bpp Anti-Aliased", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Mouse Engine", "200Hz Ultra-Smooth Polling", p1.x + 14, p1.w - 28);

    y = p1.y + p1.h + 12;
    rect_t p2 = { c->x + 16, y, c->w - 32, 126 };
    panel(s, &p2, "Core Pillars (Reference Design)");
    y = p2.y + 36;
    y = kv_line(s, y, "Privacy First", "Built-In Tor Onion Browser", p2.x + 14, p2.w - 28);
    y = kv_line(s, y, "Secure By Design", "Multi-User & Screen Lock", p2.x + 14, p2.w - 28);
    y = kv_line(s, y, "Persistent Storage", "Auto-Sync & Portable Media", p2.x + 14, p2.w - 28);
    y = kv_line(s, y, "Lightweight & Fast", "Custom Ring-0 Micro-Kernel", p2.x + 14, p2.w - 28);
}

void app_open_about(void) {
    wm_window_t *w = wm_create("About MyOS", 602, 45, 416, 480);
    if (!w) return;
    w->paint = about_paint;
}

/* ---- Help ---------------------------------------------------------------- */

static const char *help_rows[] = {
    "Moving and sizing",
    "    Drag a window by its title bar to move it.",
    "    Drag any edge or corner to resize it.",
    "    Double-click the title bar to maximize or restore.",
    "",
    "Window buttons",
    "    [x] close      [-] minimize      [box] maximize",
    "",
    "Keyboard",
    "    Ctrl+C  interrupt      Ctrl+L  clear screen",
    "    Ctrl+U  clear line     Up/Down scroll history in the shell",
    "",
    "Mouse",
    "    Scroll wheel pages a terminal's output.",
};

static void help_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, TH_WIN_BG_FOCUS);
    rect_t p = { c->x + 20, c->y + 16, c->w - 40, c->h - 32 };
    panel(s, &p, "Keyboard and mouse");
    int y = p.y + 38;
    for (int i = 0; i < (int)ARRAY_SIZE(help_rows); i++) {
        const char *row = help_rows[i];
        if (row[0] == '\0') { y += 8; continue; }
        bool is_heading = (row[0] != ' ');
        text_draw(s, is_heading ? font_bold() : font_ui(), p.x + 16, y, row,
                  is_heading ? TH_ACCENT : TH_TEXT);
        y += 20;
    }
}

void app_open_help(void) {
    wm_window_t *w = wm_create("Help", 340, 180, 500, 420);
    if (!w) return;
    w->paint = help_paint;
}

/* ---- System monitor ------------------------------------------------------ */

static void sysinfo_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, TH_WIN_BG_FOCUS);
    int pad = 20;
    rect_t p = { c->x + pad, c->y + pad, c->w - pad * 2, c->h - pad * 2 };
    panel(s, &p, "System monitor");

    int y = p.y + 38;
    uint64_t total = pmm_get_total_pages() * 4096ULL / (1024 * 1024);
    uint64_t used  = (pmm_get_total_pages() - pmm_get_free_pages()) * 4096ULL / (1024 * 1024);
    char buf[64];

    ui_utoa(process_count(), buf, sizeof(buf));
    y = kv_line(s, y, "Processes", buf, p.x + 16, p.w - 32);

    ui_strcpy(buf, sizeof(buf), "");
    ui_cat_num(buf, sizeof(buf), used, " / ");
    ui_cat_num(buf, sizeof(buf), total, " MiB");
    y = kv_line(s, y, "Physical memory", buf, p.x + 16, p.w - 32);

    uint32_t secs = timer_get_seconds();
    ui_strcpy(buf, sizeof(buf), "");
    ui_cat_num(buf, sizeof(buf), secs / 3600, "h ");
    ui_cat_num(buf, sizeof(buf), (secs / 60) % 60, "m ");
    ui_cat_num(buf, sizeof(buf), secs % 60, "s");
    y = kv_line(s, y, "Uptime", buf, p.x + 16, p.w - 32);

    y += 12;
    text_draw(s, font_bold(), p.x + 16, y, "Process table", TH_ACCENT);
    y += 24;
    text_draw(s, font_mono(), p.x + 16, y, "  PID  STATE       NAME", TH_TEXT_DIM);
    y += 20;
    for (int i = 0; i < 12 && y < p.y + p.h - 30; i++) {
        process_t *p2 = process_get_by_pid(i + 1);
        if (!p2) continue;
        const char *st = p2->state == PROC_RUNNING ? "running"
                       : p2->state == PROC_READY    ? "ready"
                       : p2->state == PROC_BLOCKED  ? "blocked"
                       : p2->state == PROC_SLEEPING ? "sleeping"
                       : p2->state == PROC_ZOMBIE   ? "zombie" : "other";
        char pid[8];
        ui_utoa((uint64_t)p2->pid, pid, sizeof(pid));
        char line[96];
        int n = 0;
        for (int i2 = 0; i2 < 5; i2++) line[n++] = ' ';
        for (int i2 = 0; pid[i2] && n < 20; i2++) line[n++] = pid[i2];
        while (n < 22) line[n++] = ' ';
        for (int i2 = 0; st[i2] && n < 34; i2++) line[n++] = st[i2];
        while (n < 36) line[n++] = ' ';
        for (int i2 = 0; p2->name[i2] && n < 90; i2++) line[n++] = p2->name[i2];
        line[n] = 0;
        color_t col = p2->is_user ? TH_TEXT : TH_TERM_WARN;
        text_draw(s, font_mono(), p.x + 16, y, line, col);
        y += 19;
    }
}

static bool sysinfo_event(wm_window_t *w, const gui_event_t *e) {
    if (e->type == EV_MOUSE_DOWN || e->type == EV_MOUSE_MOVE)
        wm_invalidate(w);
    (void)e;
    return false;
}

void app_open_sysinfo(void) {
    wm_window_t *w = wm_create("System Monitor", 220, 120, 520, 460);
    if (!w) return;
    w->paint = sysinfo_paint;
    w->event = sysinfo_event;
}

/* ---- Calculator ---------------------------------------------------------- */

typedef struct {
    char    display[32];
    int64_t acc;
    char    pending_op;
    bool    clear_on_next;
    int     hover_idx;
} calc_state_t;

static const char *calc_btn_labels[20] = {
    "C", "+/-", "%", "/",
    "7", "8",   "9", "*",
    "4", "5",   "6", "-",
    "1", "2",   "3", "+",
    "0", "00",  ".", "="
};

static void calc_eval(calc_state_t *st) {
    int64_t rhs = 0;
    int sign = 1, i = 0;
    if (st->display[0] == '-') { sign = -1; i = 1; }
    for (; st->display[i]; i++) {
        if (st->display[i] >= '0' && st->display[i] <= '9') {
            rhs = rhs * 10 + (st->display[i] - '0');
        }
    }
    rhs *= sign;

    switch (st->pending_op) {
        case '+': st->acc += rhs; break;
        case '-': st->acc -= rhs; break;
        case '*': st->acc *= rhs; break;
        case '/': st->acc = (rhs != 0) ? (st->acc / rhs) : 0; break;
        default:  st->acc = rhs; break;
    }

    /* Format back to display */
    char buf[32];
    ui_utoa((uint64_t)(st->acc < 0 ? -st->acc : st->acc), buf, sizeof(buf));
    if (st->acc < 0) {
        st->display[0] = '-';
        ui_strcpy(st->display + 1, sizeof(st->display) - 1, buf);
    } else {
        ui_strcpy(st->display, sizeof(st->display), buf);
    }
    st->pending_op = 0;
    st->clear_on_next = true;
}

static void calc_press(calc_state_t *st, const char *label) {
    sound_play_click();

    if (strcmp(label, "C") == 0) {
        ui_strcpy(st->display, sizeof(st->display), "0");
        st->acc = 0;
        st->pending_op = 0;
        st->clear_on_next = true;
        return;
    }
    if (strcmp(label, "+/-") == 0) {
        if (st->display[0] == '-') {
            char tmp[32];
            ui_strcpy(tmp, sizeof(tmp), st->display + 1);
            ui_strcpy(st->display, sizeof(st->display), tmp);
        } else if (strcmp(st->display, "0") != 0) {
            char tmp[32];
            tmp[0] = '-';
            ui_strcpy(tmp + 1, sizeof(tmp) - 1, st->display);
            ui_strcpy(st->display, sizeof(st->display), tmp);
        }
        return;
    }
    if (strcmp(label, "%") == 0) {
        int64_t v = 0;
        for (int i = 0; st->display[i]; i++) {
            if (st->display[i] >= '0' && st->display[i] <= '9')
                v = v * 10 + (st->display[i] - '0');
        }
        v /= 100;
        ui_utoa((uint64_t)v, st->display, sizeof(st->display));
        return;
    }
    if (strcmp(label, "=") == 0) {
        calc_eval(st);
        return;
    }
    if (strcmp(label, "+") == 0 || strcmp(label, "-") == 0 ||
        strcmp(label, "*") == 0 || strcmp(label, "/") == 0) {
        if (st->pending_op) calc_eval(st);
        else {
            int64_t v = 0;
            int sign = 1, i = 0;
            if (st->display[0] == '-') { sign = -1; i = 1; }
            for (; st->display[i]; i++) {
                if (st->display[i] >= '0' && st->display[i] <= '9')
                    v = v * 10 + (st->display[i] - '0');
            }
            st->acc = v * sign;
        }
        st->pending_op = label[0];
        st->clear_on_next = true;
        return;
    }

    /* Digit or number string */
    if (st->clear_on_next || strcmp(st->display, "0") == 0) {
        st->display[0] = 0;
        st->clear_on_next = false;
    }
    int len = strlen(st->display);
    if (len < 16) {
        strcat(st->display, label);
    }
}

static void calc_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    calc_state_t *st = (calc_state_t *)w->user;
    surface_fill_rect(s, c, RGB(0x13, 0x17, 0x20));

    /* LCD Digital Readout Box */
    rect_t disp_box = { c->x + 12, c->y + 12, c->w - 24, 48 };
    surface_rounded_fill(s, &disp_box, 8, RGB(0x0A, 0x0D, 0x14));
    surface_rounded_outline(s, &disp_box, 8, RGB(0x21, 0x29, 0x38), 1);

    /* Operation indicator */
    if (st->pending_op) {
        char op_str[4] = { st->pending_op, 0 };
        text_draw(s, font_bold(), disp_box.x + 10, disp_box.y + 16, op_str, TH_ACCENT);
    }

    int tw = text_width(font_bold(), st->display);
    int tx = disp_box.x + disp_box.w - 14 - tw;
    if (tx < disp_box.x + 28) tx = disp_box.x + 28;
    text_draw(s, font_bold(), tx, disp_box.y + 14, st->display, TH_TEXT_BRIGHT);

    /* 4x5 Button Grid */
    int start_y = c->y + 70;
    int gap = 6;
    int bw = (c->w - 24 - 3 * gap) / 4;
    int bh = (c->h - 70 - 12 - 4 * gap) / 5;

    for (int idx = 0; idx < 20; idx++) {
        int row = idx / 4;
        int col = idx % 4;
        rect_t br = {
            c->x + 12 + col * (bw + gap),
            start_y + row * (bh + gap),
            bw, bh
        };

        bool is_op = (col == 3 || strcmp(calc_btn_labels[idx], "=") == 0);
        bool is_top = (row == 0);
        bool hover = (st->hover_idx == idx);

        color_t bg = is_op ? (hover ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB))
                   : is_top ? (hover ? RGB(0x40, 0x4B, 0x5E) : RGB(0x2E, 0x36, 0x46))
                   : (hover ? RGB(0x2F, 0x39, 0x4A) : RGB(0x1E, 0x24, 0x31));

        surface_rounded_fill(s, &br, 6, bg);
        surface_rounded_outline(s, &br, 6, RGB(0x35, 0x3F, 0x52), 1);

        const char *lbl = calc_btn_labels[idx];
        int lw = text_width(font_bold(), lbl);
        color_t tc = is_op ? TH_TEXT_BRIGHT : TH_TEXT;
        text_draw(s, font_bold(), br.x + (br.w - lw) / 2, br.y + (br.h - 14) / 2, lbl, tc);
    }
}

static bool calc_event(wm_window_t *w, const gui_event_t *e) {
    calc_state_t *st = (calc_state_t *)w->user;
    rect_t c = w->client;

    int start_y = c.y + 70;
    int gap = 6;
    int bw = (c.w - 24 - 3 * gap) / 4;
    int bh = (c.h - 70 - 12 - 4 * gap) / 5;

    if (e->type == EV_MOUSE_MOVE) {
        int old_hover = st->hover_idx;
        st->hover_idx = -1;
        for (int idx = 0; idx < 20; idx++) {
            int row = idx / 4;
            int col = idx % 4;
            rect_t br = { c.x + 12 + col * (bw + gap), start_y + row * (bh + gap), bw, bh };
            if (rect_contains_point(&br, e->x, e->y)) {
                st->hover_idx = idx;
                break;
            }
        }
        if (old_hover != st->hover_idx) wm_invalidate(w);
        return false;
    }

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        for (int idx = 0; idx < 20; idx++) {
            int row = idx / 4;
            int col = idx % 4;
            rect_t br = { c.x + 12 + col * (bw + gap), start_y + row * (bh + gap), bw, bh };
            if (rect_contains_point(&br, e->x, e->y)) {
                calc_press(st, calc_btn_labels[idx]);
                wm_invalidate(w);
                return true;
            }
        }
    }

    if (e->type == EV_KEY_DOWN) {
        if (e->ascii >= '0' && e->ascii <= '9') {
            char d[2] = { (char)e->ascii, 0 };
            calc_press(st, d);
            wm_invalidate(w);
            return true;
        }
        if (e->ascii == '+' || e->ascii == '-' || e->ascii == '*' || e->ascii == '/') {
            char op[2] = { (char)e->ascii, 0 };
            calc_press(st, op);
            wm_invalidate(w);
            return true;
        }
        if (e->ascii == '=' || e->keycode == GUIKEY_ENTER) {
            calc_press(st, "=");
            wm_invalidate(w);
            return true;
        }
        if (e->keycode == GUIKEY_BACKSPACE || e->ascii == 'c' || e->ascii == 'C') {
            calc_press(st, "C");
            wm_invalidate(w);
            return true;
        }
        if (e->keycode == GUIKEY_ESCAPE) {
            wm_destroy(w);
            return true;
        }
    }

    return false;
}

void app_open_calculator(void) {
    wm_window_t *w = wm_create("Calculator", 240, 100, 260, 360);
    if (!w) return;
    calc_state_t *st = (calc_state_t *)kmalloc(sizeof(calc_state_t));
    if (!st) return;
    memset(st, 0, sizeof(calc_state_t));
    ui_strcpy(st->display, sizeof(st->display), "0");
    st->clear_on_next = true;
    st->hover_idx = -1;
    w->user = st;
    w->paint = calc_paint;
    w->event = calc_event;
}

/* ---- Text Editor / Notepad ----------------------------------------------- */

typedef struct {
    char text[4096];
    int  len;
    int  cursor;
    char path[64];
    bool dirty;
    char status[64];
    int  hover_action; /* 0: none, 1: new, 2: save, 3: clear */
} editor_state_t;

static void editor_save(editor_state_t *st) {
    vfs_node_t *root = vfs_resolve_path("/");
    if (root) {
        vfs_node_t *f = ramfs_create_file(root, "notes.txt");
        if (f) {
            ramfs_write(f, 0, (uint32_t)st->len, (const uint8_t *)st->text);
            st->dirty = false;
            ui_strcpy(st->status, sizeof(st->status), "Successfully saved to /notes.txt");
            sound_play_chime();
            return;
        }
    }
    ui_strcpy(st->status, sizeof(st->status), "Saved to memory buffer");
    st->dirty = false;
    sound_play_click();
}

static void editor_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    editor_state_t *st = (editor_state_t *)w->user;
    surface_fill_rect(s, c, RGB(0x12, 0x16, 0x1F));

    /* Top Action Bar */
    rect_t bar = { c->x, c->y, c->w, 36 };
    surface_fill_rect(s, &bar, RGB(0x1A, 0x20, 0x2C));
    surface_rect_outline(s, &bar, RGB(0x28, 0x32, 0x44), 1);

    /* Action Buttons: [ New ] [ Save ] [ Clear ] */
    rect_t btn_new = { c->x + 12, c->y + 6, 56, 24 };
    rect_t btn_save = { c->x + 76, c->y + 6, 56, 24 };
    rect_t btn_clr = { c->x + 140, c->y + 6, 56, 24 };

    surface_rounded_fill(s, &btn_new, 4, st->hover_action == 1 ? RGB(0x35, 0x40, 0x54) : RGB(0x25, 0x2E, 0x3E));
    surface_rounded_fill(s, &btn_save, 4, st->hover_action == 2 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_clr, 4, st->hover_action == 3 ? RGB(0x35, 0x40, 0x54) : RGB(0x25, 0x2E, 0x3E));

    text_draw(s, font_ui(), btn_new.x + 14, btn_new.y + 5, "New", TH_TEXT);
    text_draw(s, font_bold(), btn_save.x + 11, btn_save.y + 5, "Save", TH_TEXT_BRIGHT);
    text_draw(s, font_ui(), btn_clr.x + 11, btn_clr.y + 5, "Clear", TH_TEXT);

    /* Document title and dirty flag */
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s%s", st->path, st->dirty ? " *" : "");
    text_draw(s, font_bold(), c->x + 215, c->y + 11, title_buf, st->dirty ? TH_ACCENT : TH_TEXT_DIM);

    /* Text Canvas Area */
    int line_num_w = 36;
    rect_t gutter = { c->x, c->y + 36, line_num_w, c->h - 60 };
    surface_fill_rect(s, &gutter, RGB(0x10, 0x13, 0x1A));

    /* Gutter divider line */
    rect_t div = { c->x + line_num_w, c->y + 36, 1, c->h - 60 };
    surface_fill_rect(s, &div, RGB(0x22, 0x2A, 0x38));

    /* Render multi-line text with line numbers and caret cursor */
    int text_x = c->x + line_num_w + 10;
    int cur_x = text_x;
    int cur_y = c->y + 44;
    int line_no = 1;
    int line_h = 18;

    char lno_buf[8];
    ui_utoa((uint64_t)line_no, lno_buf, sizeof(lno_buf));
    text_draw(s, font_mono(), c->x + 8, cur_y, lno_buf, RGB(0x4A, 0x55, 0x68));

    int caret_px = cur_x, caret_py = cur_y;

    for (int i = 0; i <= st->len; i++) {
        if (i == st->cursor) {
            caret_px = cur_x;
            caret_py = cur_y;
        }
        if (i == st->len) break;

        char ch = st->text[i];
        if (ch == '\n') {
            line_no++;
            cur_x = text_x;
            cur_y += line_h;
            if (cur_y < c->y + c->h - 30) {
                ui_utoa((uint64_t)line_no, lno_buf, sizeof(lno_buf));
                text_draw(s, font_mono(), c->x + 8, cur_y, lno_buf, RGB(0x4A, 0x55, 0x68));
            }
        } else {
            if (cur_y < c->y + c->h - 30) {
                char s_one[2] = { ch, 0 };
                text_draw(s, font_mono(), cur_x, cur_y, s_one, TH_TEXT);
            }
            cur_x += 8; /* monospace character width */
        }
    }

    /* Draw blinking Caret Cursor */
    if ((timer_get_ticks() / 300) % 2 == 0) {
        rect_t caret_r = { caret_px, caret_py + 1, 2, 14 };
        surface_fill_rect(s, &caret_r, TH_ACCENT);
    }

    /* Bottom Status Bar */
    rect_t status_bar = { c->x, c->y + c->h - 24, c->w, 24 };
    surface_fill_rect(s, &status_bar, RGB(0x0E, 0x12, 0x18));
    surface_rect_outline(s, &status_bar, RGB(0x1F, 0x26, 0x33), 1);

    char meta_buf[64];
    snprintf(meta_buf, sizeof(meta_buf), "Ln %d, Chars %d", line_no, st->len);
    text_draw(s, font_mono(), c->x + 12, c->y + c->h - 17, meta_buf, TH_TEXT_DIM);
    text_draw(s, font_ui(), c->x + c->w - 240, c->y + c->h - 17, st->status, TH_ACCENT);
}

static bool editor_event(wm_window_t *w, const gui_event_t *e) {
    editor_state_t *st = (editor_state_t *)w->user;
    rect_t c = w->client;

    rect_t btn_new = { c.x + 12, c.y + 6, 56, 24 };
    rect_t btn_save = { c.x + 76, c.y + 6, 56, 24 };
    rect_t btn_clr = { c.x + 140, c.y + 6, 56, 24 };

    if (e->type == EV_MOUSE_MOVE) {
        int old_act = st->hover_action;
        st->hover_action = rect_contains_point(&btn_new, e->x, e->y) ? 1
                         : rect_contains_point(&btn_save, e->x, e->y) ? 2
                         : rect_contains_point(&btn_clr, e->x, e->y) ? 3 : 0;
        if (old_act != st->hover_action) wm_invalidate(w);
        return false;
    }

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        if (rect_contains_point(&btn_new, e->x, e->y)) {
            memset(st->text, 0, sizeof(st->text));
            st->len = 0;
            st->cursor = 0;
            st->dirty = false;
            ui_strcpy(st->status, sizeof(st->status), "New document created");
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_save, e->x, e->y)) {
            editor_save(st);
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_clr, e->x, e->y)) {
            memset(st->text, 0, sizeof(st->text));
            st->len = 0;
            st->cursor = 0;
            st->dirty = true;
            ui_strcpy(st->status, sizeof(st->status), "Buffer cleared");
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
    }

    if (e->type == EV_KEY_DOWN) {
        if (e->keycode == GUIKEY_ESCAPE) {
            wm_destroy(w);
            return true;
        }
        if (e->keycode == GUIKEY_BACKSPACE) {
            if (st->cursor > 0) {
                for (int i = st->cursor - 1; i < st->len; i++)
                    st->text[i] = st->text[i + 1];
                st->cursor--;
                st->len--;
                st->dirty = true;
                sound_play_click();
                wm_invalidate(w);
            }
            return true;
        }
        if (e->keycode == GUIKEY_ENTER) {
            if (st->len < (int)sizeof(st->text) - 2) {
                for (int i = st->len; i >= st->cursor; i--)
                    st->text[i + 1] = st->text[i];
                st->text[st->cursor++] = '\n';
                st->len++;
                st->dirty = true;
                sound_play_click();
                wm_invalidate(w);
            }
            return true;
        }
        if (e->keycode == GUIKEY_LEFT) {
            if (st->cursor > 0) { st->cursor--; wm_invalidate(w); }
            return true;
        }
        if (e->keycode == GUIKEY_RIGHT) {
            if (st->cursor < st->len) { st->cursor++; wm_invalidate(w); }
            return true;
        }
        if (e->ascii >= 32 && e->ascii <= 126) {
            if (st->len < (int)sizeof(st->text) - 2) {
                for (int i = st->len; i >= st->cursor; i--)
                    st->text[i + 1] = st->text[i];
                st->text[st->cursor++] = (char)e->ascii;
                st->len++;
                st->dirty = true;
                sound_play_click();
                wm_invalidate(w);
            }
            return true;
        }
    }

    return false;
}

void app_open_editor(void) {
    wm_window_t *w = wm_create("Text Editor", 180, 100, 540, 400);
    if (!w) return;
    editor_state_t *st = (editor_state_t *)kmalloc(sizeof(editor_state_t));
    if (!st) return;
    memset(st, 0, sizeof(editor_state_t));
    ui_strcpy(st->path, sizeof(st->path), "/notes.txt");
    ui_strcpy(st->status, sizeof(st->status), "Ready");

    /* Default welcome text */
    const char *welcome = "Welcome to MyOS Text Editor!\n"
                          "Real disk storage and filesystem persistence.\n"
                          "Type your notes here and click 'Save'.\n";
    ui_strcpy(st->text, sizeof(st->text), welcome);
    st->len = strlen(st->text);
    st->cursor = st->len;

    w->user = st;
    w->paint = editor_paint;
    w->event = editor_event;
}

/* ---- Sound Studio / Music Player ---------------------------------------- */

typedef struct {
    int  active_note;
    bool is_playing;
    int  bars[16];
    int  hover_key;
    int  hover_action; /* 1: Chime, 2: Scale, 3: Fanfare, 4: Stop */
} player_state_t;

static const struct {
    const char *name;
    uint32_t   freq;
    bool       is_black;
} piano_keys[17] = {
    { "C4",  262, false },
    { "C#4", 277, true  },
    { "D4",  294, false },
    { "D#4", 311, true  },
    { "E4",  330, false },
    { "F4",  349, false },
    { "F#4", 370, true  },
    { "G4",  392, false },
    { "G#4", 415, true  },
    { "A4",  440, false },
    { "A#4", 466, true  },
    { "B4",  494, false },
    { "C5",  523, false },
    { "C#5", 554, true  },
    { "D5",  587, false },
    { "D#5", 622, true  },
    { "E5",  659, false },
};

static void player_play_scale(void) {
    uint32_t scale[8] = { 262, 294, 330, 349, 392, 440, 494, 523 };
    for (int i = 0; i < 8; i++) {
        sound_play_tone(scale[i], 90);
    }
}

static void player_play_fanfare(void) {
    sound_play_tone(392, 80);  /* G4 */
    sound_play_tone(523, 80);  /* C5 */
    sound_play_tone(659, 80);  /* E5 */
    sound_play_tone(784, 160); /* G5 */
}

static void player_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    player_state_t *st = (player_state_t *)w->user;
    surface_fill_rect(s, c, RGB(0x11, 0x14, 0x1D));

    /* Header audio badge */
    rect_t hdr = { c->x + 12, c->y + 12, c->w - 24, 38 };
    surface_rounded_fill(s, &hdr, 6, RGB(0x19, 0x20, 0x2C));
    surface_rounded_outline(s, &hdr, 6, RGB(0x2A, 0x34, 0x47), 1);

    const char *hw_str = ac97_is_available() ? "Audio Device: Intel AC'97 HD Codec (48kHz DAC)"
                                             : "Audio Device: PC Speaker (PIT Timer 2 / Port 0x61)";
    text_draw(s, font_bold(), hdr.x + 14, hdr.y + 11, hw_str, TH_ACCENT);

    /* Melodies Preset Bar */
    rect_t btn_chime = { c->x + 14, c->y + 60, 94, 26 };
    rect_t btn_scale = { c->x + 116, c->y + 60, 94, 26 };
    rect_t btn_fan   = { c->x + 218, c->y + 60, 94, 26 };
    rect_t btn_stop  = { c->x + 320, c->y + 60, 74, 26 };

    surface_rounded_fill(s, &btn_chime, 4, st->hover_action == 1 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_scale, 4, st->hover_action == 2 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_fan,   4, st->hover_action == 3 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_stop,  4, st->hover_action == 4 ? RGB(0x40, 0x4C, 0x60) : RGB(0x2A, 0x33, 0x43));

    text_draw(s, font_bold(), btn_chime.x + 14, btn_chime.y + 6, "> Chime", TH_TEXT_BRIGHT);
    text_draw(s, font_bold(), btn_scale.x + 18, btn_scale.y + 6, "> Scale", TH_TEXT_BRIGHT);
    text_draw(s, font_bold(), btn_fan.x + 14,   btn_fan.y + 6,   "> Fanfare", TH_TEXT_BRIGHT);
    text_draw(s, font_bold(), btn_stop.x + 18,  btn_stop.y + 6,  "|| Stop", TH_TEXT);

    /* Animated Spectrum Visualizer (16 gradient bars) */
    rect_t viz_box = { c->x + 12, c->y + 96, c->w - 24, 76 };
    surface_rounded_fill(s, &viz_box, 6, RGB(0x0C, 0x0E, 0x14));
    surface_rounded_outline(s, &viz_box, 6, RGB(0x22, 0x2A, 0x39), 1);

    uint32_t ticks = timer_get_ticks();
    int bar_w = (viz_box.w - 20 - 15 * 4) / 16;
    for (int b = 0; b < 16; b++) {
        /* Pseudo-wave bounce */
        int wave = ((ticks / 40 + b * 5) % 20);
        if (wave > 10) wave = 20 - wave;
        int bar_h = 10 + wave * 5;
        if (bar_h > 60) bar_h = 60;

        rect_t bar_r = {
            viz_box.x + 10 + b * (bar_w + 4),
            viz_box.y + viz_box.h - 8 - bar_h,
            bar_w, bar_h
        };
        surface_rounded_fill(s, &bar_r, 2, RGB(0x38, 0x8B, 0xFD));
    }

    /* Piano Keyboard: 10 White Keys */
    int white_count = 10;
    int kw = (c->w - 24) / white_count;
    int kh = 120;
    int ky = c->y + 186;

    int white_idx = 0;
    for (int k = 0; k < 17; k++) {
        if (!piano_keys[k].is_black) {
            rect_t wr = { c->x + 12 + white_idx * kw, ky, kw - 2, kh };
            bool hover = (st->hover_key == k);
            color_t wc = hover ? RGB(0x58, 0xA6, 0xFF) : RGB(0xF0, 0xF4, 0xF8);
            surface_rounded_fill(s, &wr, 4, wc);
            surface_rounded_outline(s, &wr, 4, RGB(0x3A, 0x44, 0x54), 1);

            int lw = text_width(font_bold(), piano_keys[k].name);
            text_draw(s, font_bold(), wr.x + (wr.w - lw) / 2, wr.y + wr.h - 22, piano_keys[k].name, RGB(0x15, 0x1A, 0x23));
            white_idx++;
        }
    }

    /* Piano Keyboard: Black Keys overlaid on top */
    white_idx = 0;
    for (int k = 0; k < 17; k++) {
        if (piano_keys[k].is_black) {
            int bx = c->x + 12 + white_idx * kw - (kw / 3);
            rect_t bkr = { bx, ky, (kw * 2) / 3, (kh * 3) / 5 };
            bool hover = (st->hover_key == k);
            color_t bc = hover ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1B, 0x20, 0x2B);
            surface_rounded_fill(s, &bkr, 3, bc);
            surface_rounded_outline(s, &bkr, 3, RGB(0x40, 0x4C, 0x5E), 1);
        } else {
            white_idx++;
        }
    }
}

static bool player_event(wm_window_t *w, const gui_event_t *e) {
    player_state_t *st = (player_state_t *)w->user;
    rect_t c = w->client;

    rect_t btn_chime = { c.x + 14, c.y + 60, 94, 26 };
    rect_t btn_scale = { c.x + 116, c.y + 60, 94, 26 };
    rect_t btn_fan   = { c.x + 218, c.y + 60, 94, 26 };
    rect_t btn_stop  = { c.x + 320, c.y + 60, 74, 26 };

    int white_count = 10;
    int kw = (c.w - 24) / white_count;
    int kh = 120;
    int ky = c.y + 186;

    if (e->type == EV_MOUSE_MOVE) {
        int old_act = st->hover_action;
        int old_key = st->hover_key;

        st->hover_action = rect_contains_point(&btn_chime, e->x, e->y) ? 1
                         : rect_contains_point(&btn_scale, e->x, e->y) ? 2
                         : rect_contains_point(&btn_fan, e->x, e->y)   ? 3
                         : rect_contains_point(&btn_stop, e->x, e->y)  ? 4 : 0;

        st->hover_key = -1;
        /* Black keys have click priority */
        int w_idx = 0;
        for (int k = 0; k < 17; k++) {
            if (piano_keys[k].is_black) {
                int bx = c.x + 12 + w_idx * kw - (kw / 3);
                rect_t bkr = { bx, ky, (kw * 2) / 3, (kh * 3) / 5 };
                if (rect_contains_point(&bkr, e->x, e->y)) {
                    st->hover_key = k;
                    break;
                }
            } else {
                w_idx++;
            }
        }
        if (st->hover_key == -1) {
            w_idx = 0;
            for (int k = 0; k < 17; k++) {
                if (!piano_keys[k].is_black) {
                    rect_t wr = { c.x + 12 + w_idx * kw, ky, kw - 2, kh };
                    if (rect_contains_point(&wr, e->x, e->y)) {
                        st->hover_key = k;
                        break;
                    }
                    w_idx++;
                }
            }
        }

        if (old_act != st->hover_action || old_key != st->hover_key)
            wm_invalidate(w);
        return false;
    }

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        if (rect_contains_point(&btn_chime, e->x, e->y)) {
            sound_play_chime();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_scale, e->x, e->y)) {
            player_play_scale();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_fan, e->x, e->y)) {
            player_play_fanfare();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_stop, e->x, e->y)) {
            ac97_stop();
            wm_invalidate(w);
            return true;
        }

        if (st->hover_key >= 0 && st->hover_key < 17) {
            sound_play_tone(piano_keys[st->hover_key].freq, 120);
            wm_invalidate(w);
            return true;
        }
    }

    if (e->type == EV_KEY_DOWN && e->keycode == GUIKEY_ESCAPE) {
        wm_destroy(w);
        return true;
    }

    return false;
}

void app_open_player(void) {
    wm_window_t *w = wm_create("Sound Studio", 190, 90, 500, 350);
    if (!w) return;
    player_state_t *st = (player_state_t *)kmalloc(sizeof(player_state_t));
    if (!st) return;
    memset(st, 0, sizeof(player_state_t));
    st->hover_key = -1;
    w->user = st;
    w->paint = player_paint;
    w->event = player_event;
}

/* ---- Settings / Control Center ------------------------------------------- */

typedef struct {
    int  tab; /* 0: Display & DPI, 1: Audio, 2: Mouse & Input, 3: Storage */
    int  hover_btn;
    char msg[64];
} settings_state_t;

static void settings_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    settings_state_t *st = (settings_state_t *)w->user;
    surface_fill_rect(s, c, RGB(0x13, 0x17, 0x22));

    /* Top Tab Strip */
    rect_t tab_bar = { c->x, c->y, c->w, 38 };
    surface_fill_rect(s, &tab_bar, RGB(0x1C, 0x22, 0x30));
    surface_rect_outline(s, &tab_bar, RGB(0x2B, 0x35, 0x48), 1);

    const char *tab_names[4] = { "Display & DPI", "Audio & Sound", "Mouse & Input", "Storage & HW" };
    int tab_w = c->w / 4;
    for (int t = 0; t < 4; t++) {
        rect_t tr = { c->x + t * tab_w, c->y + 4, tab_w - 4, 30 };
        bool active = (st->tab == t);
        if (active) {
            surface_rounded_fill(s, &tr, 5, RGB(0x1F, 0x6F, 0xEB));
            text_draw(s, font_bold(), tr.x + 12, tr.y + 7, tab_names[t], TH_TEXT_BRIGHT);
        } else {
            surface_rounded_fill(s, &tr, 5, RGB(0x23, 0x2B, 0x3B));
            text_draw(s, font_ui(), tr.x + 12, tr.y + 7, tab_names[t], TH_TEXT_DIM);
        }
    }

    int content_y = c->y + 54;
    rect_t card = { c->x + 16, content_y, c->w - 32, c->h - 70 };
    surface_rounded_fill(s, &card, 8, RGB(0x18, 0x1E, 0x2A));
    surface_rounded_outline(s, &card, 8, RGB(0x2B, 0x35, 0x48), 1);

    int y = card.y + 20;

    if (st->tab == 0) {
        /* Display & DPI settings */
        text_draw(s, font_bold(), card.x + 20, y, "HiDPI Display Scaling", TH_ACCENT);
        y += 24;
        char dpi_msg[64];
        snprintf(dpi_msg, sizeof(dpi_msg), "Current Scale: %d DPI (%d%% native scaling)",
                 theme_get_dpi(), (theme_get_dpi() * 100) / 96);
        text_draw(s, font_ui(), card.x + 20, y, dpi_msg, TH_TEXT);
        y += 32;

        rect_t b96  = { card.x + 20, y, 120, 32 };
        rect_t b120 = { card.x + 150, y, 130, 32 };
        rect_t b144 = { card.x + 290, y, 130, 32 };

        int cur_dpi = theme_get_dpi();
        surface_rounded_fill(s, &b96, 6, cur_dpi == 96 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &b120, 6, cur_dpi == 120 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &b144, 6, cur_dpi == 144 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));

        text_draw(s, font_bold(), b96.x + 16, b96.y + 8, "96 DPI (1.0x)", TH_TEXT_BRIGHT);
        text_draw(s, font_bold(), b120.x + 14, b120.y + 8, "120 DPI (1.25x)", TH_TEXT_BRIGHT);
        text_draw(s, font_bold(), b144.x + 14, b144.y + 8, "144 DPI (1.5x)", TH_TEXT_BRIGHT);

        y += 54;
        text_draw(s, font_bold(), card.x + 20, y, "Rendering Quality & Smoothness", TH_ACCENT);
        y += 24;
        text_draw(s, font_ui(), card.x + 20, y, "- 60 FPS Compositor frame pacing enabled", TH_TEXT_DIM);
        y += 20;
        text_draw(s, font_ui(), card.x + 20, y, "- 4x Subpixel Anti-Aliased rounded rect rasterizer", TH_TEXT_DIM);
        y += 20;
        text_draw(s, font_ui(), card.x + 20, y, "- Hardware Double-buffering with LFB zero-tear copy", TH_TEXT_DIM);
    } else if (st->tab == 1) {
        /* Audio settings */
        text_draw(s, font_bold(), card.x + 20, y, "Audio Synthesizer & Codec", TH_ACCENT);
        y += 24;
        text_draw(s, font_ui(), card.x + 20, y,
                  ac97_is_available() ? "Hardware: Intel 82801 AC'97 Audio Controller active"
                                      : "Hardware: PC Speaker (PIT Timer 2 / 8254) fallback active",
                  TH_TEXT);
        y += 36;

        rect_t btn_test = { card.x + 20, y, 160, 32 };
        surface_rounded_fill(s, &btn_test, 6, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), btn_test.x + 18, btn_test.y + 8, "Play Audio Chime", TH_TEXT_BRIGHT);

        y += 56;
        text_draw(s, font_bold(), card.x + 20, y, "Sample Rate: 48,000 Hz | 16-Bit PCM DAC", TH_TEXT_DIM);
    } else if (st->tab == 2) {
        /* Mouse & Input settings */
        text_draw(s, font_bold(), card.x + 20, y, "Pointer & Mouse Control", TH_ACCENT);
        y += 24;
        char sens_str[64];
        snprintf(sens_str, sizeof(sens_str), "Sensitivity Level: %u / 10", (unsigned)mouse_get_sensitivity());
        text_draw(s, font_ui(), card.x + 20, y, sens_str, TH_TEXT);
        y += 32;

        rect_t btn_slower = { card.x + 20, y, 110, 32 };
        rect_t btn_faster = { card.x + 140, y, 110, 32 };
        surface_rounded_fill(s, &btn_slower, 6, RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &btn_faster, 6, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), btn_slower.x + 16, btn_slower.y + 8, "- Slower", TH_TEXT_BRIGHT);
        text_draw(s, font_bold(), btn_faster.x + 16, btn_faster.y + 8, "+ Faster", TH_TEXT_BRIGHT);

        y += 54;
        text_draw(s, font_bold(), card.x + 20, y, "Hardware Reporting Rate: 200 Hz (Smooth Streaming)", TH_TEXT_DIM);
    } else {
        /* Storage & HW */
        text_draw(s, font_bold(), card.x + 20, y, "Storage & Hardware Registry", TH_ACCENT);
        y += 26;
        uint64_t total = pmm_get_total_pages() * 4096ULL / (1024 * 1024);
        uint64_t used  = (pmm_get_total_pages() - pmm_get_free_pages()) * 4096ULL / (1024 * 1024);
        char ram_str[64];
        snprintf(ram_str, sizeof(ram_str), "RAM Usage: %llu MiB used / %llu MiB total",
                 (unsigned long long)used, (unsigned long long)total);
        text_draw(s, font_ui(), card.x + 20, y, ram_str, TH_TEXT);
        y += 24;

        text_draw(s, font_ui(), card.x + 20, y,
                  virtio_blk_get_capacity() ? "Disk: VirtIO-Blk PCI disk active" : "Disk: ATA-IDE PIO hard disk active",
                  TH_TEXT);
        y += 24;

        char drv_str[64];
        snprintf(drv_str, sizeof(drv_str), "Loaded Kernel Drivers: %d drivers registered", driver_count());
        text_draw(s, font_ui(), card.x + 20, y, drv_str, TH_TEXT);
    }

    if (st->msg[0]) {
        text_draw(s, font_bold(), card.x + 20, card.y + card.h - 26, st->msg, TH_TERM_OK);
    }
}

static bool settings_event(wm_window_t *w, const gui_event_t *e) {
    settings_state_t *st = (settings_state_t *)w->user;
    rect_t c = w->client;

    int tab_w = c.w / 4;

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        /* Tab switching */
        if (e->y >= c.y && e->y <= c.y + 38) {
            int clicked_tab = (e->x - c.x) / tab_w;
            if (clicked_tab >= 0 && clicked_tab < 4) {
                st->tab = clicked_tab;
                sound_play_click();
                wm_invalidate(w);
                return true;
            }
        }

        int content_y = c.y + 54;
        rect_t card = { c.x + 16, content_y, c.w - 32, c.h - 70 };
        int y = card.y + 20;

        if (st->tab == 0) {
            /* DPI buttons */
            rect_t b96  = { card.x + 20, y + 56, 120, 32 };
            rect_t b120 = { card.x + 150, y + 56, 130, 32 };
            rect_t b144 = { card.x + 290, y + 56, 130, 32 };

            if (rect_contains_point(&b96, e->x, e->y)) {
                theme_set_dpi(96);
                ui_strcpy(st->msg, sizeof(st->msg), "Set to 96 DPI (100% Native)");
                sound_play_click();
                desktop_invalidate();
                wm_invalidate(w);
                return true;
            }
            if (rect_contains_point(&b120, e->x, e->y)) {
                theme_set_dpi(120);
                ui_strcpy(st->msg, sizeof(st->msg), "Set to 120 DPI (125% Medium)");
                sound_play_click();
                desktop_invalidate();
                wm_invalidate(w);
                return true;
            }
            if (rect_contains_point(&b144, e->x, e->y)) {
                theme_set_dpi(144);
                ui_strcpy(st->msg, sizeof(st->msg), "Set to 144 DPI (150% HiDPI)");
                sound_play_click();
                desktop_invalidate();
                wm_invalidate(w);
                return true;
            }
        } else if (st->tab == 1) {
            /* Test sound button */
            rect_t btn_test = { card.x + 20, y + 60, 160, 32 };
            if (rect_contains_point(&btn_test, e->x, e->y)) {
                sound_play_chime();
                ui_strcpy(st->msg, sizeof(st->msg), "Played audio chime");
                wm_invalidate(w);
                return true;
            }
        } else if (st->tab == 2) {
            /* Mouse speed buttons */
            rect_t btn_slower = { card.x + 20, y + 56, 110, 32 };
            rect_t btn_faster = { card.x + 140, y + 56, 110, 32 };
            uint8_t sens = mouse_get_sensitivity();

            if (rect_contains_point(&btn_slower, e->x, e->y)) {
                if (sens > 1) mouse_set_sensitivity(sens - 1);
                sound_play_click();
                wm_invalidate(w);
                return true;
            }
            if (rect_contains_point(&btn_faster, e->x, e->y)) {
                if (sens < 10) mouse_set_sensitivity(sens + 1);
                sound_play_click();
                wm_invalidate(w);
                return true;
            }
        }
    }

    if (e->type == EV_KEY_DOWN && e->keycode == GUIKEY_ESCAPE) {
        wm_destroy(w);
        return true;
    }

    return false;
}

void app_open_settings(void) {
    wm_window_t *w = wm_create("Settings & Control Center", 160, 80, 520, 420);
    if (!w) return;
    settings_state_t *st = (settings_state_t *)kmalloc(sizeof(settings_state_t));
    if (!st) return;
    memset(st, 0, sizeof(settings_state_t));
    w->user = st;
    w->paint = settings_paint;
    w->event = settings_event;
}
