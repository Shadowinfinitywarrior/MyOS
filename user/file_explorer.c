#include "libc.h"
#include "../gui/wm.h"
#include "../drivers/framebuffer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../fs/vfs.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MAX_PATH 256
#define MAX_FILES 1024

typedef enum {
    FE_VIEW_DETAILS = 0,
    FE_VIEW_LIST,
    FE_VIEW_TILES,
    FE_VIEW_ICONS
} fe_view_mode_t;

typedef struct {
    char name[64];
    char path[MAX_PATH];
    uint32_t size;
    bool is_dir;
    bool is_selected;
    uint32_t icon_color;
} file_entry_t;

typedef struct {
    char path[MAX_PATH];
    file_entry_t files[MAX_FILES];
    int file_count;
    int selected_index;
    int scroll_offset;
    fe_view_mode_t view_mode;
    bool show_hidden;
    char search_query[64];
    int search_cursor;
    bool search_active;
} fe_state_t;

static fe_state_t g_fe = {0};
static window_t *g_win = NULL;

static const uint32_t COL_BG = 0x1A1A2E;
static const uint32_t COL_SIDEBAR = 0x161625;
static const uint32_t COL_TOOLBAR = 0x1F1F35;
static const uint32_t COL_ADDRESS_BAR = 0x252545;
static const uint32_t COL_TEXT = 0xFFFFFF;
static const uint32_t COL_TEXT_MUTED = 0x8888AA;
static const uint32_t COL_ACCENT = 0x00A4EF;
static const uint32_t COL_SELECTION = 0x0078D7;
static const uint32_t COL_BORDER = 0x333355;
static const uint32_t COL_FOLDER = 0xFFB74D;
static const uint32_t COL_FILE = 0x81D4FA;

static void draw_rect_fb(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        fb_draw_pixel(x, y + i, color);
        fb_draw_pixel(x + w - 1, y + i, color);
    }
}

static void fill_rect_fb(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_draw_pixel(x + i, y + j, color);
        }
    }
}

static void fill_round_rect_fb(int x, int y, int w, int h, int r, uint32_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    fill_rect_fb(x + r, y, w - 2 * r, h, color);
    fill_rect_fb(x, y + r, r, h - 2 * r, color);
    fill_rect_fb(x + w - r, y + r, r, h - 2 * r, color);
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            if ((r - dx) * (r - dx) + (r - dy) * (r - dy) <= r * r) {
                fb_draw_pixel(x + dx, y + dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + dy, color);
                fb_draw_pixel(x + dx, y + h - 1 - dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

static void draw_round_rect_fb(int x, int y, int w, int h, int r, uint32_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (int i = r; i < w - r; i++) {
        fb_draw_pixel(x + i, y, color);
        fb_draw_pixel(x + i, y + h - 1, color);
    }
    for (int i = r; i < h - r; i++) {
        fb_draw_pixel(x, y + i, color);
        fb_draw_pixel(x + w - 1, y + i, color);
    }
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            if ((r - dx) * (r - dx) + (r - dy) * (r - dy) <= r * r) {
                fb_draw_pixel(x + dx, y + dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + dy, color);
                fb_draw_pixel(x + dx, y + h - 1 - dy, color);
                fb_draw_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

static void draw_char_fb(int x, int y, char c, uint32_t color, int scale) {
    extern const uint8_t font_8x8[96][8];
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font_8x8[uc - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1) fb_draw_pixel(x + col, y + row, color);
                else fill_rect_fb(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_string_fb(int x, int y, const char *str, uint32_t color, int scale) {
    int cx = x;
    while (*str) {
        draw_char_fb(cx, y, *str, color, scale);
        cx += 9 * scale;
        str++;
    }
}

static int str_width_fb(const char *str, int scale) {
    int w = 0;
    while (*str) { w += 9 * scale; str++; }
    return w;
}

static void navigate_to(const char *path) {
    strncpy(g_fe.path, path, MAX_PATH - 1);
    g_fe.path[MAX_PATH - 1] = '\0';
    g_fe.file_count = 0;
    g_fe.selected_index = 0;
    g_fe.scroll_offset = 0;

    vfs_node_t *node = vfs_resolve_path(path);
    if (node && node->readdir) {
        for (uint32_t i = 0; g_fe.file_count < MAX_FILES; i++) {
            vfs_node_t *ent = node->readdir(node, i);
            if (!ent) break;

            if (!g_fe.show_hidden && ent->name[0] == '.') continue;

            file_entry_t *fe = &g_fe.files[g_fe.file_count];
            strncpy(fe->name, ent->name, 63);
            fe->name[63] = '\0';
            snprintf(fe->path, MAX_PATH, "%s/%s", path, ent->name);
            fe->size = ent->length;
            fe->is_dir = (ent->flags & VFS_DIRECTORY) != 0;
            fe->is_selected = false;
            fe->icon_color = fe->is_dir ? COL_FOLDER : COL_FILE;
            g_fe.file_count++;
        }
    }

    if (g_fe.file_count == 0) {
        file_entry_t *fe = &g_fe.files[g_fe.file_count++];
        strcpy(fe->name, "(empty)");
        strcpy(fe->path, "");
        fe->size = 0;
        fe->is_dir = false;
        fe->is_selected = false;
        fe->icon_color = COL_TEXT_MUTED;
    }
}

static void go_up() {
    char *last_slash = strrchr(g_fe.path, '/');
    if (last_slash && last_slash != g_fe.path) {
        *last_slash = '\0';
    } else if (last_slash == g_fe.path) {
        strcpy(g_fe.path, "/");
    }
    navigate_to(g_fe.path);
}

static void open_selected() {
    if (g_fe.selected_index >= 0 && g_fe.selected_index < g_fe.file_count) {
        file_entry_t *fe = &g_fe.files[g_fe.selected_index];
        if (fe->is_dir) {
            navigate_to(fe->path);
        } else {
            kprintf("[FE] Opening file: %s\n", fe->path);
        }
    }
}

static void draw_sidebar(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_SIDEBAR);
    draw_rect_fb(x + w - 1, y, 1, h, COL_BORDER);

    const char *sections[][3] = {
        {"Quick Access", "", "0"},
        {"This PC", "", "1"},
        {"Network", "", "2"},
        {"", "", ""},
        {"Desktop", "", "3"},
        {"Documents", "", "4"},
        {"Downloads", "", "5"},
        {"Pictures", "", "6"},
        {"Music", "", "7"},
        {"Videos", "", "8"},
    };

    int cy = y + 16;
    for (int i = 0; i < 10; i++) {
        if (sections[i][0][0] == '\0') {
            cy += 12;
            continue;
        }

        bool is_quick = (i == 0);
        uint32_t color = is_quick ? COL_ACCENT : COL_TEXT;

        draw_string_fb(x + 16, cy, sections[i][0], color, 1);
        cy += 28;
    }
}

static void draw_toolbar(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_TOOLBAR);
    draw_rect_fb(x, y + h - 1, w, 1, COL_BORDER);

    const char *buttons[] = {"New", "Cut", "Copy", "Paste", "Delete", "Rename", "Properties"};
    int bx = x + 12;
    for (int i = 0; i < 7; i++) {
        draw_string_fb(bx, y + 8, buttons[i], COL_TEXT, 1);
        bx += str_width_fb(buttons[i], 1) + 20;
    }

    draw_string_fb(x + w - 120, y + 8, "View: ", COL_TEXT_MUTED, 1);
    const char *views[] = {"Details", "List", "Tiles", "Icons"};
    draw_string_fb(x + w - 75, y + 8, views[g_fe.view_mode], COL_ACCENT, 1);
}

static void draw_address_bar(int x, int y, int w, int h) {
    fill_round_rect_fb(x, y, w, h, 4, COL_ADDRESS_BAR);
    draw_round_rect_fb(x, y, w, h, 4, COL_BORDER);

    char display_path[MAX_PATH];
    strcpy(display_path, g_fe.path);
    if (strcmp(display_path, "/") != 0) {
        char *last = display_path + strlen(display_path) - 1;
        while (last > display_path && *last == '/') *last-- = '\0';
    }

    char *parts[32];
    int part_count = 0;
    char *token = strtok(display_path, "/");
    while (token && part_count < 32) {
        parts[part_count++] = token;
        token = strtok(NULL, "/");
    }

    int bx = x + 12;
    if (part_count == 0 || (part_count == 1 && parts[0][0] == '\0')) {
        draw_string_fb(bx, y + 6, "This PC", COL_TEXT, 1);
        bx += str_width_fb("This PC", 1) + 4;
    } else {
        for (int i = 0; i < part_count; i++) {
            draw_string_fb(bx, y + 6, parts[i], COL_TEXT, 1);
            bx += str_width_fb(parts[i], 1) + 4;
            if (i < part_count - 1) {
                draw_string_fb(bx, y + 6, ">", COL_TEXT_MUTED, 1);
                bx += 12;
            }
        }
    }

    if (g_fe.search_active) {
        int search_x = x + w - 180;
        fill_round_rect_fb(search_x, y + 4, 170, h - 8, 4, COL_BG);
        draw_string_fb(search_x + 8, y + 6, "Search: ", COL_TEXT_MUTED, 1);
        draw_string_fb(search_x + 64, y + 6, g_fe.search_query, COL_TEXT, 1);
        if ((timer_get_ticks() / 500) % 2 == 0) {
            draw_rect_fb(search_x + 64 + str_width_fb(g_fe.search_query, 1), y + 6, 1, 14, COL_ACCENT);
        }
    }
}

static void draw_file_list(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_BG);

    int item_h = (g_fe.view_mode == FE_VIEW_DETAILS) ? 28 : 64;
    int cols = (g_fe.view_mode == FE_VIEW_ICONS || g_fe.view_mode == FE_VIEW_TILES) ? w / 80 : 1;
    int visible_rows = h / item_h;

    for (int i = 0; i < visible_rows; i++) {
        int idx = g_fe.scroll_offset + i * cols;
        if (idx >= g_fe.file_count) break;

        for (int c = 0; c < cols && idx < g_fe.file_count; c++, idx++) {
            file_entry_t *fe = &g_fe.files[idx];
            int ix = x + c * (w / cols) + 8;
            int iy = y + i * item_h + 4;
            int iw = (w / cols) - 16;

            if (fe->is_selected) {
                fill_round_rect_fb(ix - 4, iy - 2, iw + 8, item_h - 4, 4, COL_SELECTION);
            }

            if (g_fe.view_mode == FE_VIEW_DETAILS) {
                uint32_t icon_color = fe->is_dir ? COL_FOLDER :
                    (strstr(fe->name, ".exe") ? 0x90EE90 : COL_FILE);
                fill_round_rect_fb(ix, iy + 4, 20, 20, 3, icon_color);
                draw_string_fb(ix + 28, iy + 6, fe->name, fe->is_selected ? 0xFFFFFF : COL_TEXT, 1);

                char size_str[32];
                if (fe->is_dir) strcpy(size_str, "");
                else snprintf(size_str, 32, "%u KB", (fe->size + 1023) / 1024);
                draw_string_fb(ix + 300, iy + 6, size_str, COL_TEXT_MUTED, 1);

                char type_str[32];
                if (fe->is_dir) strcpy(type_str, "Folder");
                else {
                    char *dot = strrchr(fe->name, '.');
                    if (dot) snprintf(type_str, 32, "%s File", dot + 1);
                    else strcpy(type_str, "File");
                }
                draw_string_fb(ix + 450, iy + 6, type_str, COL_TEXT_MUTED, 1);
            } else {
                uint32_t icon_color = fe->is_dir ? COL_FOLDER : COL_FILE;
                fill_round_rect_fb(ix + 20, iy, 40, 40, 4, icon_color);
                draw_string_fb(ix + 5, iy + 48, fe->name, fe->is_selected ? 0xFFFFFF : COL_TEXT, 1);
            }
        }
    }

    if (g_fe.file_count > visible_rows * cols) {
        int sb_x = x + w - 12;
        int sb_h = h * visible_rows * cols / g_fe.file_count;
        int sb_y = y + h * g_fe.scroll_offset / g_fe.file_count;
        fill_round_rect_fb(sb_x, sb_y, 8, sb_h, 4, COL_ACCENT);
    }
}

static void draw_status_bar(int x, int y, int w, int h) {
    fill_rect_fb(x, y, w, h, COL_TOOLBAR);
    draw_rect_fb(x, y, w, 1, COL_BORDER);

    int selected = 0;
    for (int i = 0; i < g_fe.file_count; i++) {
        if (g_fe.files[i].is_selected) selected++;
    }

    char status[128];
    snprintf(status, 128, "%d items  |  %d selected", g_fe.file_count, selected);
    draw_string_fb(x + 12, y + 6, status, COL_TEXT_MUTED, 1);
}

static void draw_window_content(window_t *win) {
    (void)win;
    int sidebar_w = 220;
    int toolbar_h = 40;
    int address_h = 36;
    int status_h = 28;

    int x = 0;
    int y = 0;
    int w = g_win->width;
    int h = g_win->height;

    draw_sidebar(x, y, sidebar_w, h);
    draw_toolbar(x + sidebar_w, y, w - sidebar_w, toolbar_h);
    draw_address_bar(x + sidebar_w, y + toolbar_h, w - sidebar_w, address_h);
    draw_file_list(x + sidebar_w, y + toolbar_h + address_h, w - sidebar_w, h - toolbar_h - address_h - status_h);
    draw_status_bar(x + sidebar_w, y + h - status_h, w - sidebar_w, status_h);
}

/* ============================================================
 * Window-Manager-driven interface
 * ============================================================ */

static void fe_update(void);
static void fe_draw(void);
static void fe_handle_key_down(int key);
static void fe_handle_key_up(int key);
static void fe_handle_mouse_move(int x, int y);
static void fe_handle_mouse_down(int x, int y, int button);
static void fe_handle_mouse_up(int x, int y, int button);
static void fe_handle_mouse_wheel(int delta);

static void fe_wm_draw(struct window *win) {
    (void)win;
    fe_update();
    fe_draw();
}

static void fe_wm_key_down(struct window *win, int key) {
    (void)win;
    fe_handle_key_down(key);
}

static void fe_wm_key_up(struct window *win, int key) {
    (void)win;
    fe_handle_key_up(key);
}

static void fe_wm_mouse_down(struct window *win, int x, int y, int button) {
    (void)win;
    fe_handle_mouse_down(x, y, button);
}

static void fe_wm_mouse_up(struct window *win, int x, int y, int button) {
    (void)win;
    fe_handle_mouse_up(x, y, button);
}

static void fe_wm_mouse_move(struct window *win, int x, int y) {
    (void)win;
    fe_handle_mouse_move(x, y);
}

static void fe_wm_mouse_wheel(struct window *win, int delta) {
    (void)win;
    fe_handle_mouse_wheel(delta);
}

static void fe_init_window(void) {
    fb_info_t *fb = fb_get_info();
    int win_w = 900;
    int win_h = 600;
    int win_x = (fb->width - win_w) / 2;
    int win_y = (fb->height - win_h) / 2;

    g_win = wm_get_window(wm_create_window("File Explorer", win_x, win_y, win_w, win_h, 0));
    if (!g_win) return;
    wm_focus_window(g_win->id);

    g_win->user_data = &g_fe;

    g_win->draw_content = fe_wm_draw;
    g_win->on_mouse_down = fe_wm_mouse_down;
    g_win->on_mouse_up = fe_wm_mouse_up;
    g_win->on_mouse_move = fe_wm_mouse_move;
    g_win->on_mouse_wheel = fe_wm_mouse_wheel;
    g_win->on_key_down = fe_wm_key_down;
    g_win->on_key_up = fe_wm_key_up;
}

static void fe_update(void) {
}

static void fe_draw(void) {
    if (!g_win) return;
    draw_window_content(g_win);
}

static void fe_handle_mouse_down(int mx, int my, int button) {
    int sidebar_w = 220;
    int toolbar_h = 40;
    int address_h = 36;

    if (button == 1) {
        if (mx >= sidebar_w && mx < g_win->width && my >= toolbar_h && my < toolbar_h + address_h) {
            g_fe.search_active = true;
            g_fe.search_query[0] = '\0';
            g_fe.search_cursor = 0;
        } else {
            g_fe.search_active = false;
        }

        if (my >= toolbar_h + address_h && my < g_win->height - 28) {
            int rel_y = my - (toolbar_h + address_h);
            int item_h = 28;
            int idx = g_fe.scroll_offset + rel_y / item_h;
            if (idx >= 0 && idx < g_fe.file_count) {
                for (int i = 0; i < g_fe.file_count; i++) {
                    g_fe.files[i].is_selected = false;
                }
                g_fe.files[idx].is_selected = true;
                g_fe.selected_index = idx;
            }
        }

        if (my >= toolbar_h && my < toolbar_h + address_h) {
            int rel_x = mx - sidebar_w;
            if (rel_x > g_win->width - sidebar_w - 180) {
                g_fe.search_active = true;
            }
        }
    } else if (button == 2) {
        if (my >= toolbar_h + address_h) {
            int rel_y = my - (toolbar_h + address_h);
            int item_h = 28;
            int idx = g_fe.scroll_offset + rel_y / item_h;
            if (idx >= 0 && idx < g_fe.file_count) {
                kprintf("[FE] Context menu for: %s\n", g_fe.files[idx].name);
            }
        }
    }
}

static void fe_handle_mouse_up(int mx, int my, int button) {
    (void)mx; (void)my;
    if (button == 1 && g_fe.selected_index >= 0) {
        int click_time = timer_get_ticks();
        static int last_click = 0;
        static int last_idx = -1;

        if (click_time - last_click < 300 && last_idx == g_fe.selected_index) {
            open_selected();
        }
        last_click = click_time;
        last_idx = g_fe.selected_index;
    }
}

static void fe_handle_mouse_wheel(int delta) {
    if (delta > 0 && g_fe.scroll_offset > 0) {
        g_fe.scroll_offset--;
    } else if (delta < 0 && g_fe.scroll_offset < g_fe.file_count - 1) {
        g_fe.scroll_offset++;
    }
}

static void fe_handle_key_down(int key) {
    if (g_fe.search_active) {
        if (key == 0x1C) {
            g_fe.search_active = false;
            kprintf("[FE] Search: %s\n", g_fe.search_query);
        } else if (key == 0x0E && g_fe.search_cursor > 0) {
            g_fe.search_cursor--;
            int len = strlen(g_fe.search_query);
            memmove(&g_fe.search_query[g_fe.search_cursor], &g_fe.search_query[g_fe.search_cursor + 1], len - g_fe.search_cursor);
            g_fe.search_query[len - 1] = '\0';
        } else {
            uint8_t mods = keyboard_get_modifiers();
            bool ctrl = mods & KMOD_CTRL;
            bool alt = mods & KMOD_ALT;
            if (!ctrl && !alt && key >= 0x20 && key <= 0x7E && g_fe.search_cursor < 63) {
                char c = keyboard_keycode_to_ascii(key, mods);
                if (c) {
                    int len = strlen(g_fe.search_query);
                    memmove(&g_fe.search_query[g_fe.search_cursor + 1], &g_fe.search_query[g_fe.search_cursor], len - g_fe.search_cursor + 1);
                    g_fe.search_query[g_fe.search_cursor] = c;
                    g_fe.search_cursor++;
                }
            }
        }
        return;
    }

    switch (key) {
        case 0x48: // Up
            if (g_fe.selected_index > 0) {
                g_fe.files[g_fe.selected_index].is_selected = false;
                g_fe.selected_index--;
                g_fe.files[g_fe.selected_index].is_selected = true;
                if (g_fe.selected_index < g_fe.scroll_offset) g_fe.scroll_offset = g_fe.selected_index;
            }
            break;
        case 0x50: // Down
            if (g_fe.selected_index < g_fe.file_count - 1) {
                g_fe.files[g_fe.selected_index].is_selected = false;
                g_fe.selected_index++;
                g_fe.files[g_fe.selected_index].is_selected = true;
                if (g_fe.selected_index >= g_fe.scroll_offset + 20) g_fe.scroll_offset++;
            }
            break;
        case 0x4B: // Left - go up
            go_up();
            break;
        case 0x4D: // Right - enter
            open_selected();
            break;
        case 0x1C: // Enter
            open_selected();
            break;
        case 0x01: // Escape
            if (g_win) wm_destroy_window(g_win->id);
            g_win = NULL;
            break;
        case 0x3F: // F5 - refresh
            navigate_to(g_fe.path);
            break;
    }
}

static void fe_handle_key_up(int key) {
    (void)key;
}

static void fe_handle_mouse_move(int x, int y) {
    (void)x; (void)y;
}

void file_explorer_main(void) {
    static bool inited = false;
    if (inited) return;
    inited = true;

    memset(&g_fe, 0, sizeof(fe_state_t));
    navigate_to("/");
    if (g_fe.file_count > 0) {
        g_fe.files[0].is_selected = true;
    }

    fe_init_window();

    kprintf("[FE] File Explorer started\n");
}