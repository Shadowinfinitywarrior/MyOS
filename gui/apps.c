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
#include "../kernel/power_net.h"
#include "desktop.h"

#include "glass.h"

/* A titled "card" panel inside a window, styled with modern glassmorphism */
static void panel(surface_t *s, const rect_t *area, const char *title) {
    glass_surface_card(s, area, title);
}

static int kv_line(surface_t *s, int y, const char *k, const char *v, int x, int w) {
    text_draw(s, font_ui(), x, y, k, TH_TEXT_SECONDARY);
    text_draw(s, font_mono(), x + w - text_width(font_mono(), v), y, v, TH_TEXT_PRIMARY);
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

/* ---- Modern File Manager (NovaFS & Debian Package Integration) --------- */

extern int pkg_install_deb_file(const char *filepath);

typedef struct {
    const char *name;
    const char *path;
    const char *badge;
    color_t     color;
} file_place_t;

static const file_place_t file_places[] = {
    { "NovaFS",   "/nova",                   "NOVA", RGB(0x00, 0xE5, 0xFF) },
    { "Root",     "/",                       "ROOT", RGB(0x7A, 0xA2, 0xF7) },
    { "Binaries", "/bin",                    "BIN",  RGB(0x2E, 0xCC, 0x71) },
    { "Packages", "/var/cache/apt/archives", "DEB",  RGB(0xE7, 0x4C, 0x3C) },
    { "Docs",     "/nova/docs",              "DOC",  RGB(0xF1, 0xC4, 0x0F) },
    { "Devices",  "/dev",                    "DEV",  RGB(0xBB, 0x9A, 0xF7) }
};
#define FILE_PLACES_COUNT 6

typedef struct files_state {
    char        path[128];
    vfs_node_t *node;
    int         sel;
    int         sidebar_sel;
    uint32_t    last_click_time;
    int         last_click_row;
    char        status_msg[96];
    char        selected_filename[64];
    bool        selected_is_deb;
} files_state_t;

static void files_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    files_state_t *st = (files_state_t *)w->user;
    if (!st) return;

    /* Base Window Background */
    surface_fill_rect(s, c, RGB(0x13, 0x16, 0x20));

    /* 1. Header & Navigation Toolbar (y: 0 to 36) */
    rect_t hdr = { c->x, c->y, c->w, 36 };
    surface_fill_rect(s, &hdr, RGB(0x1A, 0x1F, 0x2C));
    rect_t hdr_line = { c->x, c->y + 35, c->w, 1 };
    surface_fill_rect(s, &hdr_line, RGB(0x2B, 0x33, 0x47));

    /* "Up" Navigation Button */
    rect_t btn_up = { c->x + 8, c->y + 6, 32, 24 };
    surface_rounded_fill(s, &btn_up, 4, RGB(0x25, 0x2C, 0x3D));
    surface_rounded_outline(s, &btn_up, 4, RGB(0x3B, 0x46, 0x5E), 1);
    text_draw(s, font_bold(), btn_up.x + 10, btn_up.y + 4, "^", RGB(0x00, 0xD2, 0xFF));

    /* Refresh Button */
    rect_t btn_ref = { c->x + 44, c->y + 6, 28, 24 };
    surface_rounded_fill(s, &btn_ref, 4, RGB(0x25, 0x2C, 0x3D));
    surface_rounded_outline(s, &btn_ref, 4, RGB(0x3B, 0x46, 0x5E), 1);
    text_draw(s, font_bold(), btn_ref.x + 8, btn_ref.y + 4, "o", RGB(0x2E, 0xCC, 0x71));

    /* Path Breadcrumb Box */
    int path_w = c->w - 200;
    if (path_w < 120) path_w = 120;
    rect_t pathbar = { c->x + 76, c->y + 6, path_w, 24 };
    surface_rounded_fill(s, &pathbar, 5, RGB(0x0F, 0x12, 0x19));
    surface_rounded_outline(s, &pathbar, 5, RGB(0x2F, 0x3A, 0x50), 1);
    text_draw(s, font_mono(), pathbar.x + 8, pathbar.y + 5, st->path, RGB(0x00, 0xE5, 0xFF));

    /* "Install .deb" Action Button */
    rect_t btn_inst = { c->x + 80 + path_w, c->y + 6, c->w - (88 + path_w), 24 };
    if (btn_inst.w > 40) {
        color_t inst_bg = st->selected_is_deb ? RGB(0x1B, 0x6E, 0x3E) : RGB(0x22, 0x28, 0x38);
        color_t inst_txt = st->selected_is_deb ? RGB(0x52, 0xE8, 0x8C) : RGB(0x6E, 0x7E, 0x94);
        surface_rounded_fill(s, &btn_inst, 4, inst_bg);
        surface_rounded_outline(s, &btn_inst, 4, st->selected_is_deb ? RGB(0x2E, 0xCC, 0x71) : RGB(0x35, 0x40, 0x54), 1);
        text_draw(s, font_bold(), btn_inst.x + (btn_inst.w - 76) / 2, btn_inst.y + 5, "Install .deb", inst_txt);
    }

    /* 2. Left Places / Bookmarks Sidebar (Width: 140px) */
    int sb_w = 140;
    rect_t sidebar = { c->x, c->y + 36, sb_w, c->h - 60 };
    surface_fill_rect(s, &sidebar, RGB(0x15, 0x18, 0x22));
    rect_t sb_line = { c->x + sb_w - 1, c->y + 36, 1, c->h - 60 };
    surface_fill_rect(s, &sb_line, RGB(0x25, 0x2D, 0x3D));

    text_draw(s, font_bold(), c->x + 12, c->y + 46, "PLACES", RGB(0x6A, 0x7B, 0x94));

    int s_y = c->y + 66;
    for (int i = 0; i < FILE_PLACES_COUNT; i++) {
        rect_t item = { c->x + 6, s_y, sb_w - 12, 26 };
        bool active = (strcmp(st->path, file_places[i].path) == 0);
        if (active) {
            surface_rounded_fill(s, &item, 5, RGB(0x23, 0x2C, 0x3F));
            surface_rounded_outline(s, &item, 5, RGB(0x00, 0xD2, 0xFF), 1);
        }

        /* Badge */
        rect_t badge = { item.x + 6, item.y + 5, 34, 16 };
        surface_rounded_fill(s, &badge, 3, RGB(0x10, 0x14, 0x1D));
        text_draw(s, font_mono(), badge.x + 4, badge.y + 2, file_places[i].badge, file_places[i].color);

        text_draw(s, font_ui(), item.x + 46, item.y + 5, file_places[i].name,
                  active ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x9E, 0xAE, 0xC4));

        s_y += 30;
    }

    /* 3. Main File List View */
    int main_x = c->x + sb_w;
    int main_w = c->w - sb_w;
    int main_y = c->y + 36;
    int main_h = c->h - 60;

    /* Column Header */
    rect_t list_hdr = { main_x, main_y, main_w, 22 };
    surface_fill_rect(s, &list_hdr, RGB(0x18, 0x1D, 0x28));
    text_draw(s, font_bold(), main_x + 12, main_y + 4, "Name", RGB(0x76, 0x88, 0xA2));
    text_draw(s, font_bold(), main_x + main_w - 180, main_y + 4, "Type", RGB(0x76, 0x88, 0xA2));
    text_draw(s, font_bold(), main_x + main_w - 75, main_y + 4, "Size", RGB(0x76, 0x88, 0xA2));

    if (!st->node) {
        text_draw(s, font_ui(), main_x + 20, main_y + 36, "Cannot open directory", RGB(0xE7, 0x4C, 0x3C));
        return;
    }

    int row_y = main_y + 26;
    int row_h = 24;
    int max_rows = (main_h - 30) / row_h;
    int idx = 0;
    int total_files = 0;

    for (int i = 0; st->node->readdir; i++) {
        vfs_node_t *child = st->node->readdir(st->node, (uint32_t)i);
        if (!child) break;
        total_files++;

        if (idx < max_rows) {
            bool is_sel = (idx == st->sel);
            if (is_sel) {
                rect_t hl = { main_x + 4, row_y - 2, main_w - 8, row_h };
                surface_rounded_fill(s, &hl, 4, RGB(0x22, 0x2E, 0x44));
                surface_rounded_outline(s, &hl, 4, RGB(0x00, 0xD2, 0xFF), 1);
                strncpy(st->selected_filename, child->name, sizeof(st->selected_filename) - 1);
                st->selected_filename[sizeof(st->selected_filename) - 1] = '\0';
                st->selected_is_deb = (strstr(child->name, ".deb") != NULL);
            }

            bool is_dir = (child->flags & VFS_DIRECTORY) != 0;
            bool is_deb = (strstr(child->name, ".deb") != NULL);
            bool is_exec = (!is_dir && !is_deb && (strcmp(st->path, "/bin") == 0 || strcmp(st->path, "/nova/apps") == 0));

            /* Colorful File Type Icons */
            if (is_dir) {
                /* Folder Icon */
                rect_t folder = { main_x + 12, row_y + 4, 14, 10 };
                surface_rounded_fill(s, &folder, 2, RGB(0xFA, 0xB0, 0x05));
                rect_t tab = { main_x + 13, row_y + 2, 6, 4 };
                surface_fill_rect(s, &tab, RGB(0xFA, 0xB0, 0x05));
            } else if (is_deb) {
                /* Package Icon */
                rect_t box = { main_x + 12, row_y + 2, 13, 13 };
                surface_rounded_fill(s, &box, 2, RGB(0xE7, 0x4C, 0x3C));
                text_draw(s, font_mono(), main_x + 14, row_y + 3, "P", RGB(0xFF, 0xFF, 0xFF));
            } else if (is_exec) {
                /* Executable Binary Icon */
                rect_t exe = { main_x + 12, row_y + 2, 13, 13 };
                surface_rounded_fill(s, &exe, 3, RGB(0x2E, 0xCC, 0x71));
                text_draw(s, font_bold(), main_x + 15, row_y + 2, "*", RGB(0xFF, 0xFF, 0xFF));
            } else {
                /* Document File Icon */
                rect_t page = { main_x + 13, row_y + 2, 11, 13 };
                surface_rounded_fill(s, &page, 1, RGB(0x3B, 0x82, 0xF6));
            }

            /* Filename */
            color_t name_color = is_deb ? RGB(0xFF, 0x7B, 0x72) : (is_dir ? RGB(0xFF, 0xE0, 0x82) : RGB(0xF0, 0xF6, 0xFC));
            text_draw(s, font_ui(), main_x + 34, row_y + 2, child->name, name_color);

            /* File Type String */
            const char *type_str = is_dir ? "Folder" : (is_deb ? "Debian Package" : (is_exec ? "Executable" : "Document"));
            text_draw(s, font_ui(), main_x + main_w - 180, row_y + 2, type_str, RGB(0x7D, 0x8D, 0xA2));

            /* Size String */
            char sizebuf[32];
            if (!is_dir) {
                if (child->length >= 1024 * 1024)
                    snprintf(sizebuf, sizeof(sizebuf), "%u MB", child->length / (1024 * 1024));
                else if (child->length >= 1024)
                    snprintf(sizebuf, sizeof(sizebuf), "%u KB", child->length / 1024);
                else
                    snprintf(sizebuf, sizeof(sizebuf), "%u B", child->length);
            } else {
                strcpy(sizebuf, "--");
            }
            text_draw(s, font_mono(), main_x + main_w - 75, row_y + 2, sizebuf, RGB(0x7D, 0x8D, 0xA2));

            row_y += row_h;
            idx++;
        }
    }

    if (total_files == 0) {
        text_draw(s, font_ui(), main_x + 20, main_y + 36, "(Folder is empty)", RGB(0x6E, 0x7E, 0x94));
    }

    /* 4. Footer & Status Bar */
    rect_t footer = { c->x, c->y + c->h - 24, c->w, 24 };
    surface_fill_rect(s, &footer, RGB(0x16, 0x19, 0x24));
    rect_t ft_line = { c->x, c->y + c->h - 24, c->w, 1 };
    surface_fill_rect(s, &ft_line, RGB(0x28, 0x30, 0x42));

    if (st->status_msg[0] != '\0') {
        text_draw(s, font_bold(), c->x + 12, footer.y + 4, st->status_msg, RGB(0x2E, 0xCC, 0x71));
    } else if (strncmp(st->path, "/nova", 5) == 0) {
        text_draw(s, font_ui(), c->x + 12, footer.y + 4,
                  "Volume: NovaFS Extent Volume (4 MB Unified Filesystem)", RGB(0x00, 0xD2, 0xFF));
    } else {
        text_draw(s, font_ui(), c->x + 12, footer.y + 4, "Ready", RGB(0x7D, 0x8D, 0xA2));
    }

    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d items", total_files);
    text_draw(s, font_ui(), c->x + c->w - 80, footer.y + 4, count_str, RGB(0x7D, 0x8D, 0xA2));
}

static bool files_event(wm_window_t *w, const gui_event_t *e) {
    files_state_t *st = (files_state_t *)w->user;
    if (!st || !e) return false;
    rect_t c;
    wm_client_screen_rect(w, &c);

    if (e->type == EV_MOUSE_DOWN) {
        int mx = e->x, my = e->y;

        /* Click on "Up" button (c->x + 8, c->y + 6, 32, 24) */
        if (mx >= c.x + 8 && mx <= c.x + 40 && my >= c.y + 6 && my <= c.y + 30) {
            int n = (int)strlen(st->path);
            while (n > 1 && st->path[n - 1] != '/') n--;
            if (n > 1) n--;
            st->path[n] = '\0';
            if (st->path[0] == '\0') strcpy(st->path, "/");
            st->node = vfs_resolve_path(st->path);
            st->sel = 0;
            st->status_msg[0] = '\0';
            wm_invalidate(w);
            return true;
        }

        /* Click on Refresh button (c->x + 44, c->y + 6, 28, 24) */
        if (mx >= c.x + 44 && mx <= c.x + 72 && my >= c.y + 6 && my <= c.y + 30) {
            st->node = vfs_resolve_path(st->path);
            st->status_msg[0] = '\0';
            wm_invalidate(w);
            return true;
        }

        /* Click on "Install .deb" button */
        int path_w = c.w - 200;
        if (path_w < 120) path_w = 120;
        rect_t btn_inst = { c.x + 80 + path_w, c.y + 6, c.w - (88 + path_w), 24 };
        if (rect_contains_point(&btn_inst, mx, my) && st->selected_is_deb) {
            char deb_path[160];
            path_join(deb_path, sizeof(deb_path), st->path, st->selected_filename);
            pkg_install_deb_file(deb_path);
            snprintf(st->status_msg, sizeof(st->status_msg), "Package installed: %s", st->selected_filename);
            wm_invalidate(w);
            return true;
        }

        /* Click on Left Places / Bookmarks Sidebar (x < c.x + 140) */
        if (mx >= c.x && mx < c.x + 140 && my >= c.y + 66 && my < c.y + 66 + FILE_PLACES_COUNT * 30) {
            int place_idx = (my - (c.y + 66)) / 30;
            if (place_idx >= 0 && place_idx < FILE_PLACES_COUNT) {
                strncpy(st->path, file_places[place_idx].path, sizeof(st->path) - 1);
                st->path[sizeof(st->path) - 1] = '\0';
                st->node = vfs_resolve_path(st->path);
                st->sel = 0;
                st->status_msg[0] = '\0';
                wm_invalidate(w);
                return true;
            }
        }

        /* Click on File Row in Main List View */
        int main_x = c.x + 140;
        int list_y0 = c.y + 36 + 26;
        if (mx >= main_x && mx < c.x + c.w && my >= list_y0 && my < c.y + c.h - 24) {
            int row = (my - list_y0) / 24;
            if (st->node && st->node->readdir) {
                vfs_node_t *child = st->node->readdir(st->node, (uint32_t)row);
                if (child) {
                    uint32_t now = timer_get_ticks();
                    bool is_double_click = (row == st->last_click_row && (now - st->last_click_time < 35));
                    st->last_click_time = now;
                    st->last_click_row = row;
                    st->sel = row;

                    if (is_double_click) {
                        if (child->flags & VFS_DIRECTORY) {
                            /* Open directory */
                            char newpath[128];
                            path_join(newpath, sizeof(newpath), st->path, child->name);
                            vfs_node_t *n = vfs_resolve_path(newpath);
                            if (n) {
                                strncpy(st->path, newpath, sizeof(st->path) - 1);
                                st->path[sizeof(st->path) - 1] = '\0';
                                st->node = n;
                                st->sel = 0;
                                st->status_msg[0] = '\0';
                            }
                        } else if (strstr(child->name, ".deb") != NULL) {
                            /* Double click on .deb installs the package! */
                            char deb_path[160];
                            path_join(deb_path, sizeof(deb_path), st->path, child->name);
                            pkg_install_deb_file(deb_path);
                            snprintf(st->status_msg, sizeof(st->status_msg), "Installed: %s", child->name);
                        }
                    }
                    wm_invalidate(w);
                    return true;
                }
            }
        }
    }

    return false;
}

void app_open_files(void) {
    wm_window_t *w = wm_create("Files — Storage & Package Manager", 160, 120, 560, 400);
    if (!w) return;
    files_state_t *st = kzalloc(sizeof(files_state_t));
    if (!st) { wm_destroy(w); return; }
    strcpy(st->path, "/nova");
    st->node = vfs_resolve_path("/nova");
    if (!st->node) {
        strcpy(st->path, "/");
        st->node = vfs_resolve_path("/");
    }
    w->user = st;
    w->paint = files_paint;
    w->event = files_event;
}

/* ---- About --------------------------------------------------------------- */

static void about_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, TH_GLASS_PRIMARY);

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
    surface_fill_rect(s, c, TH_GLASS_SECONDARY);
    rect_t p = { c->x + 20, c->y + 16, c->w - 40, c->h - 32 };
    panel(s, &p, "Keyboard and mouse");
    int y = p.y + 38;
    for (int i = 0; i < (int)ARRAY_SIZE(help_rows); i++) {
        const char *row = help_rows[i];
        if (row[0] == '\0') { y += 8; continue; }
        bool is_heading = (row[0] != ' ');
        text_draw(s, is_heading ? font_bold() : font_ui(), p.x + 16, y, row,
                  is_heading ? TH_ACCENT_PRIMARY : TH_TEXT_PRIMARY);
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
    surface_fill_rect(s, c, TH_GLASS_SECONDARY);
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
    text_draw(s, font_bold(), p.x + 16, y, "Process table", TH_ACCENT_PRIMARY);
    y += 24;
    text_draw(s, font_mono(), p.x + 16, y, "  PID  STATE       NAME", TH_TEXT_SECONDARY);
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
        color_t col = p2->is_user ? TH_TEXT_PRIMARY : TH_TERM_WARN;
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
        text_draw(s, font_bold(), disp_box.x + 10, disp_box.y + 16, op_str, TH_ACCENT_PRIMARY);
    }

    int tw = text_width(font_bold(), st->display);
    int tx = disp_box.x + disp_box.w - 14 - tw;
    if (tx < disp_box.x + 28) tx = disp_box.x + 28;
    text_draw(s, font_bold(), tx, disp_box.y + 14, st->display, TH_TEXT_PRIMARY);

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
        color_t tc = is_op ? TH_TEXT_PRIMARY : TH_TEXT_PRIMARY;
        text_draw(s, font_bold(), br.x + (br.w - lw) / 2, br.y + (br.h - 14) / 2, lbl, tc);
    }
}

static bool calc_event(wm_window_t *w, const gui_event_t *e) {
    calc_state_t *st = (calc_state_t *)w->user;
    rect_t c;
    wm_client_screen_rect(w, &c);

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

    text_draw(s, font_ui(), btn_new.x + 14, btn_new.y + 5, "New", TH_TEXT_PRIMARY);
    text_draw(s, font_bold(), btn_save.x + 11, btn_save.y + 5, "Save", TH_TEXT_PRIMARY);
    text_draw(s, font_ui(), btn_clr.x + 11, btn_clr.y + 5, "Clear", TH_TEXT_PRIMARY);

    /* Document title and dirty flag */
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s%s", st->path, st->dirty ? " *" : "");
    text_draw(s, font_bold(), c->x + 215, c->y + 11, title_buf, st->dirty ? TH_ACCENT_PRIMARY : TH_TEXT_SECONDARY);

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
                text_draw(s, font_mono(), cur_x, cur_y, s_one, TH_TEXT_PRIMARY);
            }
            cur_x += 8; /* monospace character width */
        }
    }

    /* Draw blinking Caret Cursor */
    if ((timer_get_ticks() / 300) % 2 == 0) {
        rect_t caret_r = { caret_px, caret_py + 1, 2, 14 };
        surface_fill_rect(s, &caret_r, TH_ACCENT_PRIMARY);
    }

    /* Bottom Status Bar */
    rect_t status_bar = { c->x, c->y + c->h - 24, c->w, 24 };
    surface_fill_rect(s, &status_bar, RGB(0x0E, 0x12, 0x18));
    surface_rect_outline(s, &status_bar, RGB(0x1F, 0x26, 0x33), 1);

    char meta_buf[64];
    snprintf(meta_buf, sizeof(meta_buf), "Ln %d, Chars %d", line_no, st->len);
    text_draw(s, font_mono(), c->x + 12, c->y + c->h - 17, meta_buf, TH_TEXT_SECONDARY);
    text_draw(s, font_ui(), c->x + c->w - 240, c->y + c->h - 17, st->status, TH_ACCENT_PRIMARY);
}

static bool editor_event(wm_window_t *w, const gui_event_t *e) {
    editor_state_t *st = (editor_state_t *)w->user;
    rect_t c;
    wm_client_screen_rect(w, &c);

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
    text_draw(s, font_bold(), hdr.x + 14, hdr.y + 11, hw_str, TH_ACCENT_PRIMARY);

    /* Melodies Preset Bar */
    rect_t btn_chime = { c->x + 14, c->y + 60, 94, 26 };
    rect_t btn_scale = { c->x + 116, c->y + 60, 94, 26 };
    rect_t btn_fan   = { c->x + 218, c->y + 60, 94, 26 };
    rect_t btn_stop  = { c->x + 320, c->y + 60, 74, 26 };

    surface_rounded_fill(s, &btn_chime, 4, st->hover_action == 1 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_scale, 4, st->hover_action == 2 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_fan,   4, st->hover_action == 3 ? RGB(0x38, 0x8B, 0xFD) : RGB(0x1F, 0x6F, 0xEB));
    surface_rounded_fill(s, &btn_stop,  4, st->hover_action == 4 ? RGB(0x40, 0x4C, 0x60) : RGB(0x2A, 0x33, 0x43));

    text_draw(s, font_bold(), btn_chime.x + 14, btn_chime.y + 6, "> Chime", TH_TEXT_PRIMARY);
    text_draw(s, font_bold(), btn_scale.x + 18, btn_scale.y + 6, "> Scale", TH_TEXT_PRIMARY);
    text_draw(s, font_bold(), btn_fan.x + 14,   btn_fan.y + 6,   "> Fanfare", TH_TEXT_PRIMARY);
    text_draw(s, font_bold(), btn_stop.x + 18,  btn_stop.y + 6,  "|| Stop", TH_TEXT_PRIMARY);

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
    rect_t c;
    wm_client_screen_rect(w, &c);

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
            text_draw(s, font_bold(), tr.x + 12, tr.y + 7, tab_names[t], TH_TEXT_PRIMARY);
        } else {
            surface_rounded_fill(s, &tr, 5, RGB(0x23, 0x2B, 0x3B));
            text_draw(s, font_ui(), tr.x + 12, tr.y + 7, tab_names[t], TH_TEXT_SECONDARY);
        }
    }

    int content_y = c->y + 54;
    rect_t card = { c->x + 16, content_y, c->w - 32, c->h - 70 };
    surface_rounded_fill(s, &card, 8, RGB(0x18, 0x1E, 0x2A));
    surface_rounded_outline(s, &card, 8, RGB(0x2B, 0x35, 0x48), 1);

    int y = card.y + 20;

    if (st->tab == 0) {
        /* Display & DPI settings */
        text_draw(s, font_bold(), card.x + 20, y, "HiDPI Display Scaling", TH_ACCENT_PRIMARY);
        y += 24;
        char dpi_msg[64];
        snprintf(dpi_msg, sizeof(dpi_msg), "Current Scale: %d DPI (%d%% native scaling)",
                 theme_get_dpi(), (theme_get_dpi() * 100) / 96);
        text_draw(s, font_ui(), card.x + 20, y, dpi_msg, TH_TEXT_PRIMARY);
        y += 32;

        rect_t b96  = { card.x + 20, y, 120, 32 };
        rect_t b120 = { card.x + 150, y, 130, 32 };
        rect_t b144 = { card.x + 290, y, 130, 32 };

        int cur_dpi = theme_get_dpi();
        surface_rounded_fill(s, &b96, 6, cur_dpi == 96 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &b120, 6, cur_dpi == 120 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &b144, 6, cur_dpi == 144 ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));

        text_draw(s, font_bold(), b96.x + 16, b96.y + 8, "96 DPI (1.0x)", TH_TEXT_PRIMARY);
        text_draw(s, font_bold(), b120.x + 14, b120.y + 8, "120 DPI (1.25x)", TH_TEXT_PRIMARY);
        text_draw(s, font_bold(), b144.x + 14, b144.y + 8, "144 DPI (1.5x)", TH_TEXT_PRIMARY);

        y += 48;
        text_draw(s, font_bold(), card.x + 20, y, "Glassmorphism & Visual Effects", TH_ACCENT_PRIMARY);
        y += 24;

        rect_t b_blur = { card.x + 20, y, 170, 30 };
        rect_t b_motion = { card.x + 205, y, 185, 30 };
        bool blur_on = glass_get_blur_enabled();
        bool motion_red = glass_get_reduced_motion();

        surface_rounded_fill(s, &b_blur, 6, blur_on ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_outline(s, &b_blur, 6, blur_on ? RGB(0x65, 0xD9, 0xFF) : RGB(0x45, 0x54, 0x6E), 1);
        text_draw(s, font_bold(), b_blur.x + 16, b_blur.y + 7, blur_on ? "Glass Blur: ON" : "Glass Blur: OFF", TH_TEXT_PRIMARY);

        surface_rounded_fill(s, &b_motion, 6, motion_red ? RGB(0x1F, 0x6F, 0xEB) : RGB(0x2C, 0x36, 0x48));
        surface_rounded_outline(s, &b_motion, 6, motion_red ? RGB(0x65, 0xD9, 0xFF) : RGB(0x45, 0x54, 0x6E), 1);
        text_draw(s, font_bold(), b_motion.x + 16, b_motion.y + 7, motion_red ? "Reduced Motion: ON" : "Reduced Motion: OFF", TH_TEXT_PRIMARY);

        y += 44;
        text_draw(s, font_bold(), card.x + 20, y, "Rendering Quality & Smoothness", TH_ACCENT_PRIMARY);
        y += 20;
        text_draw(s, font_ui(), card.x + 20, y, "- 60 FPS Compositor frame pacing enabled", TH_TEXT_SECONDARY);
        y += 18;
        text_draw(s, font_ui(), card.x + 20, y, "- 4x Subpixel Anti-Aliased rounded rect rasterizer", TH_TEXT_SECONDARY);
        y += 18;
        text_draw(s, font_ui(), card.x + 20, y, "- Hardware Double-buffering with LFB zero-tear copy", TH_TEXT_SECONDARY);
    } else if (st->tab == 1) {
        /* Audio settings */
        text_draw(s, font_bold(), card.x + 20, y, "Audio Synthesizer & Codec", TH_ACCENT_PRIMARY);
        y += 24;
        text_draw(s, font_ui(), card.x + 20, y,
                  ac97_is_available() ? "Hardware: Intel 82801 AC'97 Audio Controller active"
                                      : "Hardware: PC Speaker (PIT Timer 2 / 8254) fallback active",
                  TH_TEXT_PRIMARY);
        y += 36;

        rect_t btn_test = { card.x + 20, y, 160, 32 };
        surface_rounded_fill(s, &btn_test, 6, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), btn_test.x + 18, btn_test.y + 8, "Play Audio Chime", TH_TEXT_PRIMARY);

        y += 56;
        text_draw(s, font_bold(), card.x + 20, y, "Sample Rate: 48,000 Hz | 16-Bit PCM DAC", TH_TEXT_SECONDARY);
    } else if (st->tab == 2) {
        /* Mouse & Input settings */
        text_draw(s, font_bold(), card.x + 20, y, "Pointer & Mouse Control", TH_ACCENT_PRIMARY);
        y += 24;
        char sens_str[64];
        snprintf(sens_str, sizeof(sens_str), "Sensitivity Level: %u / 10", (unsigned)mouse_get_sensitivity());
        text_draw(s, font_ui(), card.x + 20, y, sens_str, TH_TEXT_PRIMARY);
        y += 32;

        rect_t btn_slower = { card.x + 20, y, 110, 32 };
        rect_t btn_faster = { card.x + 140, y, 110, 32 };
        surface_rounded_fill(s, &btn_slower, 6, RGB(0x2C, 0x36, 0x48));
        surface_rounded_fill(s, &btn_faster, 6, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), btn_slower.x + 16, btn_slower.y + 8, "- Slower", TH_TEXT_PRIMARY);
        text_draw(s, font_bold(), btn_faster.x + 16, btn_faster.y + 8, "+ Faster", TH_TEXT_PRIMARY);

        y += 54;
        text_draw(s, font_bold(), card.x + 20, y, "Hardware Reporting Rate: 200 Hz (Smooth Streaming)", TH_TEXT_SECONDARY);
    } else {
        /* Storage & HW */
        text_draw(s, font_bold(), card.x + 20, y, "Storage & Hardware Registry", TH_ACCENT_PRIMARY);
        y += 26;
        uint64_t total = pmm_get_total_pages() * 4096ULL / (1024 * 1024);
        uint64_t used  = (pmm_get_total_pages() - pmm_get_free_pages()) * 4096ULL / (1024 * 1024);
        char ram_str[64];
        snprintf(ram_str, sizeof(ram_str), "RAM Usage: %llu MiB used / %llu MiB total",
                 (unsigned long long)used, (unsigned long long)total);
        text_draw(s, font_ui(), card.x + 20, y, ram_str, TH_TEXT_PRIMARY);
        y += 24;

        text_draw(s, font_ui(), card.x + 20, y,
                  virtio_blk_get_capacity() ? "Disk: VirtIO-Blk PCI disk active" : "Disk: ATA-IDE PIO hard disk active",
                  TH_TEXT_PRIMARY);
        y += 24;

        char drv_str[64];
        snprintf(drv_str, sizeof(drv_str), "Loaded Kernel Drivers: %d drivers registered", driver_count());
        text_draw(s, font_ui(), card.x + 20, y, drv_str, TH_TEXT_PRIMARY);
    }

    if (st->msg[0]) {
        text_draw(s, font_bold(), card.x + 20, card.y + card.h - 26, st->msg, TH_TERM_OK);
    }
}

static bool settings_event(wm_window_t *w, const gui_event_t *e) {
    settings_state_t *st = (settings_state_t *)w->user;
    rect_t c;
    wm_client_screen_rect(w, &c);

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

            rect_t b_blur = { card.x + 20, y + 128, 170, 30 };
            rect_t b_motion = { card.x + 205, y + 128, 185, 30 };
            if (rect_contains_point(&b_blur, e->x, e->y)) {
                glass_set_blur_enabled(!glass_get_blur_enabled());
                ui_strcpy(st->msg, sizeof(st->msg), glass_get_blur_enabled() ? "Frosted Glass Blur Enabled" : "Frosted Glass Blur Disabled");
                sound_play_click();
                desktop_invalidate();
                wm_invalidate_all();
                return true;
            }
            if (rect_contains_point(&b_motion, e->x, e->y)) {
                glass_set_reduced_motion(!glass_get_reduced_motion());
                ui_strcpy(st->msg, sizeof(st->msg), glass_get_reduced_motion() ? "Reduced Motion Enabled" : "Reduced Motion Disabled");
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

/* ---- Calendar App ------------------------------------------------------- */

typedef struct cal_state {
    int year;
    int month; /* 1 - 12 */
    int cur_year;
    int cur_month;
    int cur_day;
} cal_state_t;

static int is_leap_year(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static int days_in_month(int y, int m) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2) return 28 + is_leap_year(y);
    if (m >= 1 && m <= 12) return d[m - 1];
    return 30;
}

static int first_day_of_month(int y, int m) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y/4 - y/100 + y/400 + t[m-1] + 1) % 7;
}

static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static void cal_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    cal_state_t *st = (cal_state_t *)w->user;
    surface_fill_rect(s, c, RGB(0x13, 0x17, 0x22));

    /* Header with Month Year and Prev/Next buttons */
    rect_t hdr = { c->x + 10, c->y + 8, c->w - 20, 34 };
    surface_rounded_fill(s, &hdr, 7, RGB(0x1C, 0x24, 0x36));
    surface_rounded_outline(s, &hdr, 7, RGB(0x32, 0x42, 0x5E), 1);

    rect_t btn_prev = { hdr.x + 8, hdr.y + 5, 28, 24 };
    rect_t btn_next = { hdr.x + hdr.w - 36, hdr.y + 5, 28, 24 };
    surface_rounded_fill(s, &btn_prev, 4, RGB(0x28, 0x36, 0x4D));
    surface_rounded_fill(s, &btn_next, 4, RGB(0x28, 0x36, 0x4D));
    text_draw(s, font_bold(), btn_prev.x + 9, btn_prev.y + 4, "<", TH_TEXT_PRIMARY);
    text_draw(s, font_bold(), btn_next.x + 10, btn_next.y + 4, ">", TH_TEXT_PRIMARY);

    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s %d", month_names[st->month - 1], st->year);
    int tw = text_width(font_bold(), title_buf);
    text_draw(s, font_bold(), hdr.x + (hdr.w - tw) / 2, hdr.y + 8, title_buf, TH_TEXT_PRIMARY);

    /* Weekday columns */
    static const char *wk_names[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    int col_w = (c->w - 24) / 7;
    int row_h = 26;
    int start_y = c->y + 50;

    for (int i = 0; i < 7; i++) {
        int wx = c->x + 12 + i * col_w;
        color_t col = (i == 0 || i == 6) ? RGB(0x6A, 0x8C, 0xBE) : RGB(0x8A, 0xA2, 0xCE);
        int ww = text_width(font_bold(), wk_names[i]);
        text_draw(s, font_bold(), wx + (col_w - ww) / 2, start_y, wk_names[i], col);
    }

    /* Day numbers */
    int fday = first_day_of_month(st->year, st->month);
    int total_days = days_in_month(st->year, st->month);
    int cell_y = start_y + 22;

    for (int d = 1; d <= total_days; d++) {
        int slot = fday + d - 1;
        int row = slot / 7;
        int col = slot % 7;
        int dx = c->x + 12 + col * col_w;
        int dy = cell_y + row * row_h;

        bool is_today = (st->year == st->cur_year && st->month == st->cur_month && d == st->cur_day);

        if (is_today) {
            rect_t pill = { dx + (col_w - 24) / 2, dy - 2, 24, 22 };
            surface_rounded_fill(s, &pill, 11, TH_ACCENT_PRIMARY);
        }

        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d", d);
        int nw = text_width(is_today ? font_bold() : font_ui(), num_str);
        color_t tc = is_today ? RGB(255, 255, 255) : TH_TEXT_PRIMARY;
        text_draw(s, is_today ? font_bold() : font_ui(), dx + (col_w - nw) / 2, dy + 2, num_str, tc);
    }

    /* Footer: Current accurate RTC time */
    datetime_t dt;
    rtc_get_time(&dt);
    char foot_buf[64];
    snprintf(foot_buf, sizeof(foot_buf), "Clock Sync: %02u:%02u:%02u UTC", dt.hour, dt.minute, dt.second);
    int fw = text_width(font_mono(), foot_buf);
    rect_t foot_box = { c->x + 12, c->y + c->h - 30, c->w - 24, 22 };
    surface_rounded_fill(s, &foot_box, 5, RGB(0x18, 0x1E, 0x2C));
    text_draw(s, font_mono(), foot_box.x + (foot_box.w - fw) / 2, foot_box.y + 4, foot_buf, RGB(0x42, 0xB0, 0xEF));
}

static bool cal_event(wm_window_t *w, const gui_event_t *e) {
    cal_state_t *st = (cal_state_t *)w->user;
    rect_t c;
    wm_client_screen_rect(w, &c);

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        rect_t hdr = { c.x + 10, c.y + 8, c.w - 20, 34 };
        rect_t btn_prev = { hdr.x + 8, hdr.y + 5, 28, 24 };
        rect_t btn_next = { hdr.x + hdr.w - 36, hdr.y + 5, 28, 24 };

        if (rect_contains_point(&btn_prev, e->x, e->y)) {
            st->month--;
            if (st->month < 1) {
                st->month = 12;
                st->year--;
            }
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_next, e->x, e->y)) {
            st->month++;
            if (st->month > 12) {
                st->month = 1;
                st->year++;
            }
            sound_play_click();
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

void app_open_calendar(void) {
    wm_window_t *existing = wm_find("Calendar");
    if (existing) {
        wm_focus(existing);
        return;
    }

    datetime_t dt;
    rtc_get_time(&dt);

    wm_window_t *w = wm_create("Calendar", 260, 80, 340, 300);
    if (!w) return;
    cal_state_t *st = (cal_state_t *)kmalloc(sizeof(cal_state_t));
    if (!st) return;
    st->year = dt.year ? dt.year : 2026;
    st->month = dt.month ? dt.month : 10;
    st->cur_year = st->year;
    st->cur_month = st->month;
    st->cur_day = dt.day ? dt.day : 8;

    w->user = st;
    w->paint = cal_paint;
    w->event = cal_event;
}

/* ---- Network Manager App ------------------------------------------------ */

static void net_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, RGB(0x13, 0x17, 0x22));

    net_state_t net;
    net_get_state(&net);

    int card_w = c->w - 24;

    /* Card 1: Ethernet */
    rect_t card_eth = { c->x + 12, c->y + 12, card_w, 76 };
    surface_rounded_fill(s, &card_eth, 8, RGB(0x1B, 0x22, 0x30));
    surface_rounded_outline(s, &card_eth, 8, RGB(0x2D, 0x3A, 0x52), 1);
    text_draw(s, font_bold(), card_eth.x + 12, card_eth.y + 10, "Ethernet (eth0)", TH_TEXT_PRIMARY);
    text_draw(s, font_ui(), card_eth.x + 12, card_eth.y + 30, net.eth_connected ? "Status: Connected (1000 Mbps Full Duplex)" : "Status: Cable Disconnected", net.eth_connected ? RGB(50, 205, 120) : TH_TEXT_SECONDARY);
    char eth_ip_str[64];
    snprintf(eth_ip_str, sizeof(eth_ip_str), "IPv4: %s  |  Gateway: 10.0.2.2", net.eth_ip);
    text_draw(s, font_mono(), card_eth.x + 12, card_eth.y + 50, eth_ip_str, TH_TEXT_SECONDARY);

    rect_t btn_eth = { card_eth.x + card_eth.w - 96, card_eth.y + 10, 84, 24 };
    surface_rounded_fill(s, &btn_eth, 5, net.eth_connected ? RGB(0x3B, 0x50, 0x72) : RGB(0x2A, 0x7B, 0x45));
    text_draw(s, font_ui(), btn_eth.x + 10, btn_eth.y + 4, net.eth_connected ? "Disconnect" : "Connect", TH_TEXT_PRIMARY);

    /* Card 2: Wi-Fi */
    rect_t card_wifi = { c->x + 12, c->y + 96, card_w, 82 };
    surface_rounded_fill(s, &card_wifi, 8, RGB(0x1B, 0x22, 0x30));
    surface_rounded_outline(s, &card_wifi, 8, RGB(0x2D, 0x3A, 0x52), 1);
    text_draw(s, font_bold(), card_wifi.x + 12, card_wifi.y + 10, "Wi-Fi (wlan0) - Intel 802.11ac", TH_TEXT_PRIMARY);
    char wifi_st[96];
    if (net.wifi_enabled) {
        snprintf(wifi_st, sizeof(wifi_st), "Connected to: %s (%d%% Signal, %d dBm)", net.wifi_ssid, net.wifi_signal_pct, net.wifi_dbm);
    } else {
        snprintf(wifi_st, sizeof(wifi_st), "Wi-Fi Hardware Disabled");
    }
    text_draw(s, font_ui(), card_wifi.x + 12, card_wifi.y + 30, wifi_st, net.wifi_enabled ? RGB(50, 205, 120) : TH_TEXT_SECONDARY);
    text_draw(s, font_mono(), card_wifi.x + 12, card_wifi.y + 50, "Security: WPA3-Personal  |  Band: 5.0 GHz (Ch 36)", TH_TEXT_SECONDARY);

    rect_t btn_wifi = { card_wifi.x + card_wifi.w - 84, card_wifi.y + 10, 72, 24 };
    surface_rounded_fill(s, &btn_wifi, 5, net.wifi_enabled ? RGB(0x84, 0x32, 0x32) : RGB(0x2A, 0x7B, 0x45));
    text_draw(s, font_ui(), btn_wifi.x + 12, btn_wifi.y + 4, net.wifi_enabled ? "Turn Off" : "Turn On", TH_TEXT_PRIMARY);

    /* Card 3: Bluetooth */
    rect_t card_bt = { c->x + 12, c->y + 186, card_w, 76 };
    surface_rounded_fill(s, &card_bt, 8, RGB(0x1B, 0x22, 0x30));
    surface_rounded_outline(s, &card_bt, 8, RGB(0x2D, 0x3A, 0x52), 1);
    text_draw(s, font_bold(), card_bt.x + 12, card_bt.y + 10, "Bluetooth (bt0) - Intel BT 5.3 LE", TH_TEXT_PRIMARY);
    text_draw(s, font_ui(), card_bt.x + 12, card_bt.y + 30, net.bt_enabled ? "Status: Enabled (Discoverable)" : "Status: Disabled", net.bt_enabled ? RGB(65, 160, 245) : TH_TEXT_SECONDARY);
    char bt_dev_str[64];
    snprintf(bt_dev_str, sizeof(bt_dev_str), "Paired: %s (%d devices)", net.bt_device, net.bt_paired_count);
    text_draw(s, font_mono(), card_bt.x + 12, card_bt.y + 50, bt_dev_str, TH_TEXT_SECONDARY);

    rect_t btn_bt = { card_bt.x + card_bt.w - 84, card_bt.y + 10, 72, 24 };
    surface_rounded_fill(s, &btn_bt, 5, net.bt_enabled ? RGB(0x84, 0x32, 0x32) : RGB(0x2A, 0x7B, 0x45));
    text_draw(s, font_ui(), btn_bt.x + 12, btn_bt.y + 4, net.bt_enabled ? "Turn Off" : "Turn On", TH_TEXT_PRIMARY);
}

static bool net_event(wm_window_t *w, const gui_event_t *e) {
    rect_t c;
    wm_client_screen_rect(w, &c);
    int card_w = c.w - 24;

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        rect_t btn_eth = { c.x + 12 + card_w - 96, c.y + 12 + 10, 84, 24 };
        rect_t btn_wifi = { c.x + 12 + card_w - 84, c.y + 96 + 10, 72, 24 };
        rect_t btn_bt = { c.x + 12 + card_w - 84, c.y + 186 + 10, 72, 24 };

        if (rect_contains_point(&btn_eth, e->x, e->y)) {
            net_toggle_eth();
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_wifi, e->x, e->y)) {
            net_toggle_wifi();
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_bt, e->x, e->y)) {
            net_toggle_bt();
            sound_play_click();
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

void app_open_network(void) {
    wm_window_t *existing = wm_find("Network Connections");
    if (existing) {
        wm_focus(existing);
        return;
    }
    wm_window_t *w = wm_create("Network Connections", 180, 80, 440, 290);
    if (!w) return;
    w->paint = net_paint;
    w->event = net_event;
}

/* ---- Power & Battery Manager App ---------------------------------------- */

static void power_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    (void)w;
    surface_fill_rect(s, c, RGB(0x13, 0x17, 0x22));

    power_state_t p;
    power_get_state(&p);

    int card_w = c->w - 24;

    /* Card 1: Battery & Charger status */
    rect_t card_bat = { c->x + 12, c->y + 12, card_w, 110 };
    surface_rounded_fill(s, &card_bat, 8, RGB(0x1B, 0x22, 0x30));
    surface_rounded_outline(s, &card_bat, 8, RGB(0x2D, 0x3A, 0x52), 1);

    text_draw(s, font_bold(), card_bat.x + 14, card_bat.y + 10, "ACPI Battery & Power Source", TH_TEXT_PRIMARY);

    /* Visual Battery Gauge */
    rect_t b_frame = { card_bat.x + 14, card_bat.y + 36, 56, 24 };
    surface_rounded_outline(s, &b_frame, 4, RGB(140, 160, 190), 2);
    rect_t b_nub = { b_frame.x + b_frame.w, b_frame.y + 6, 3, 12 };
    surface_rounded_fill(s, &b_nub, 2, RGB(140, 160, 190));

    int fill_w = (50 * (int)p.battery_percent) / 100;
    if (fill_w < 2) fill_w = 2;
    rect_t b_fill = { b_frame.x + 3, b_frame.y + 3, fill_w, 18 };
    color_t fill_col = p.is_charging ? RGB(45, 200, 110)
                     : (p.battery_percent > 35 ? RGB(45, 200, 110) : (p.battery_percent > 15 ? RGB(235, 175, 40) : RGB(235, 60, 60)));
    surface_rounded_fill(s, &b_fill, 2, fill_col);

    char pct_buf[32];
    snprintf(pct_buf, sizeof(pct_buf), "%u%% %s", p.battery_percent, p.is_charging ? "(Charging Mode)" : "(Discharging)");
    text_draw(s, font_bold(), card_bat.x + 84, card_bat.y + 34, pct_buf, p.is_charging ? RGB(50, 220, 130) : TH_TEXT_PRIMARY);

    char detail_buf[96];
    snprintf(detail_buf, sizeof(detail_buf), "Voltage: %u mV  |  Health: 99%% (4,850 mAh / 4,900 mAh)", p.voltage_mv);
    text_draw(s, font_mono(), card_bat.x + 84, card_bat.y + 54, detail_buf, TH_TEXT_SECONDARY);

    text_draw(s, font_ui(), card_bat.x + 14, card_bat.y + 82, p.status_str, TH_ACCENT_PRIMARY);

    /* Card 2: Controls */
    rect_t card_ctrl = { c->x + 12, c->y + 130, card_w, 86 };
    surface_rounded_fill(s, &card_ctrl, 8, RGB(0x1B, 0x22, 0x30));
    surface_rounded_outline(s, &card_ctrl, 8, RGB(0x2D, 0x3A, 0x52), 1);

    text_draw(s, font_bold(), card_ctrl.x + 14, card_ctrl.y + 10, "Power State Actions", TH_TEXT_PRIMARY);

    rect_t btn_ac = { card_ctrl.x + 14, card_ctrl.y + 36, 150, 32 };
    surface_rounded_fill(s, &btn_ac, 6, p.is_charging ? RGB(0x28, 0x58, 0x3C) : RGB(0x3B, 0x50, 0x72));
    text_draw(s, font_ui(), btn_ac.x + 12, btn_ac.y + 8, p.is_charging ? "Unplug AC Charger" : "Plug In AC Charger", TH_TEXT_PRIMARY);

    rect_t btn_sleep = { card_ctrl.x + 176, card_ctrl.y + 36, 160, 32 };
    surface_rounded_fill(s, &btn_sleep, 6, RGB(0x56, 0x38, 0x82));
    text_draw(s, font_ui(), btn_sleep.x + 14, btn_sleep.y + 8, "Enter Standby Mode", TH_TEXT_PRIMARY);
}

static bool power_event(wm_window_t *w, const gui_event_t *e) {
    rect_t c;
    wm_client_screen_rect(w, &c);

    if (e->type == EV_MOUSE_DOWN && e->button == 0) {
        rect_t btn_ac = { c.x + 12 + 14, c.y + 130 + 36, 150, 32 };
        rect_t btn_sleep = { c.x + 12 + 176, c.y + 130 + 36, 160, 32 };

        if (rect_contains_point(&btn_ac, e->x, e->y)) {
            power_toggle_charging();
            sound_play_click();
            wm_invalidate(w);
            return true;
        }
        if (rect_contains_point(&btn_sleep, e->x, e->y)) {
            power_set_standby(true);
            sound_play_click();
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

void app_open_power(void) {
    wm_window_t *existing = wm_find("Power & Battery Manager");
    if (existing) {
        wm_focus(existing);
        return;
    }
    wm_window_t *w = wm_create("Power & Battery Manager", 200, 90, 400, 240);
    if (!w) return;
    w->paint = power_paint;
    w->event = power_event;
}

