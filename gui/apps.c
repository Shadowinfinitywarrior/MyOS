#include "apps.h"
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
    rect_t band = { c->x, c->y, c->w, 86 };
    surface_gradient_v(s, &band, RGB(0x24, 0x33, 0x50), RGB(0x14, 0x1B, 0x26));
    rect_t band_line = { c->x, c->y + 85, c->w, 1 };
    surface_fill_rect(s, &band_line, RGB(0x30, 0x38, 0x48));

    /* Modern logo icon badge */
    rect_t badge = { c->x + 18, c->y + 18, 48, 48 };
    surface_rounded_fill(s, &badge, 12, RGB(0x38, 0x8B, 0xFD));
    surface_rounded_outline(s, &badge, 12, RGB(0x79, 0xB8, 0xFF), 1);
    /* Atom / shield symbol */
    rect_t inner = { c->x + 32, c->y + 32, 20, 20 };
    surface_rounded_fill(s, &inner, 10, RGB(0xFF, 0xFF, 0xFF));
    rect_t core = { c->x + 38, c->y + 38, 8, 8 };
    surface_rounded_fill(s, &core, 4, RGB(0x1F, 0x6F, 0xEB));

    text_draw(s, font_bold(), c->x + 76, c->y + 22, "MyOS Modern Desktop", TH_TEXT_BRIGHT);
    text_draw(s, font_ui(), c->x + 76, c->y + 46, "64-bit Unix OS • Multi-Language GUI", TH_TITLE_TEXT_DIM);

    int y = c->y + 102;
    rect_t p1 = { c->x + 16, y, c->w - 32, 134 };
    panel(s, &p1, "System Information");
    y = p1.y + 36;
    y = kv_line(s, y, "Architecture", "x86-64 Long Mode", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Kernel", "SMP Preemptive Ring 0", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Memory Space", "Higher-Half Paged", p1.x + 14, p1.w - 28);
    y = kv_line(s, y, "Display Engine", "1024x768 32bpp", p1.x + 14, p1.w - 28);

    y = p1.y + p1.h + 14;
    rect_t p2 = { c->x + 16, y, c->w - 32, 114 };
    panel(s, &p2, "Modern Architecture");
    y = p2.y + 36;
    y = kv_line(s, y, "Core Compositor", "Rust Bare-Metal", p2.x + 14, p2.w - 28);
    y = kv_line(s, y, "Desktop Shell", "Go / C Window Manager", p2.x + 14, p2.w - 28);
    y = kv_line(s, y, "App Runtimes", "Java & Python", p2.x + 14, p2.w - 28);
}

void app_open_about(void) {
    wm_window_t *w = wm_create("About MyOS", 612, 55, 400, 460);
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
