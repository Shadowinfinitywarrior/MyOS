#include "wm.h"
#include "../drivers/framebuffer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../kernel/timer.h"
#include "../kernel/heap.h"
#include "../kernel/process.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* Kernel-compatible function aliases */
#define sleep_ms(ms)    timer_sleep(ms)
#define yield()         process_yield()
#define puts(s)         kprintf("%s\n", s)
#define putchar(c)      kprintf("%c", c)
#define printf_simple   kprintf

/* Font from wm.c */
extern const uint8_t font_8x8[96][8];

/* ============================================================
 * Terminal Configuration
 * ============================================================ */
#define TERM_MAX_TABS             16
#define TERM_TAB_HEIGHT           32
#define TERM_TAB_MIN_WIDTH        100
#define TERM_TAB_MAX_WIDTH        200
#define TERM_NEW_TAB_BTN_W        28
#define TERM_SCROLLBACK_LINES     10000
#define TERM_MAX_LINE_LEN         4096
#define TERM_HISTORY_SIZE         1000
#define TERM_MAX_COMPLETIONS      64
#define TERM_COMPLETION_POPUP_H   200
#define TERM_CURSOR_BLINK_MS      530

/* Color Scheme - Windows 11 Dark Mode with Acrylic */
#define COL_TERM_BG               0x1A1A2E
#define COL_TERM_BG_ALT           0x1E1E3A
#define COL_TAB_BAR_BG            0x161628
#define COL_TAB_ACTIVE_BG         0x1A1A2E
#define COL_TAB_INACTIVE_BG       0x161628
#define COL_TAB_HOVER_BG          0x20203A
#define COL_TAB_BORDER            0x2A2A4A
#define COL_TAB_TEXT_ACTIVE       0xFFFFFF
#define COL_TAB_TEXT_INACTIVE     0xAAAAAA
#define COL_ACCENT_BLUE           0x00A4EF
#define COL_ACCENT_HOVER          0x0078D7
#define COL_TEXT_PRIMARY          0xFFFFFF
#define COL_TEXT_SECONDARY        0xBBBBBB
#define COL_TEXT_MUTED            0x666666
#define COL_CURSOR                0x00A4EF
#define COL_CURSOR_TEXT           0x1A1A2E
#define COL_SELECTION_BG          0x0078D7
#define COL_SELECTION_TEXT        0xFFFFFF
#define COL_SCROLLBAR_BG          0x161628
#define COL_SCROLLBAR_THUMB       0x444466
#define COL_SCROLLBAR_HOVER       0x555588
#define COL_PROMPT_USER           0x00A4EF
#define COL_PROMPT_HOST           0x00CC66
#define COL_PROMPT_PATH           0xFFB800
#define COL_PROMPT_SYMBOL         0xFFFFFF
#define COL_CMD_BUILTIN           0x00CC66
#define COL_CMD_EXTERNAL          0x00A4EF
#define COL_CMD_ERROR             0xFF6666
#define COL_OUTPUT_STDOUT         0xE0E0E0
#define COL_OUTPUT_STDERR         0xFF8888
#define COL_BORDER                0x2A2A4A
#define COL_SHADOW                0x000000

/* Built-in Commands */
#define CMD_BUILTIN_COUNT         18

/* Tab States */
typedef enum {
    TAB_STATE_NORMAL = 0,
    TAB_STATE_HOVERED,
    TAB_STATE_ACTIVE,
    TAB_STATE_BELL
} term_tab_state_t;

/* Terminal Line Structure */
typedef struct term_line {
    char text[TERM_MAX_LINE_LEN];
    int len;
    uint32_t *colors;  /* Per-character colors, NULL for default */
    bool wrapped;
} term_line_t;

/* Tab Structure */
typedef struct term_tab {
    int id;
    char title[64];
    char cwd[256];
    int pid;                    /* Child process PID */
    int pty_fd;                 /* PTY file descriptor */
    
    /* Scrollback buffer */
    term_line_t *scrollback;
    int scrollback_size;
    int scrollback_capacity;
    int scrollback_start;       /* Index of oldest line */
    int scrollback_end;         /* Index after newest line */
    int scroll_offset;          /* Lines scrolled up from bottom */
    
    /* Input line */
    char input_buffer[TERM_MAX_LINE_LEN];
    int input_len;
    int cursor_pos;
    int cursor_col;             /* Visual column (accounts for wide chars) */
    
    /* History */
    char *history[TERM_HISTORY_SIZE];
    int history_count;
    int history_pos;            /* Current position in history (-1 = current input) */
    
    /* Selection */
    bool selecting;
    int sel_start_x, sel_start_y;
    int sel_end_x, sel_end_y;
    bool has_selection;
    
    /* Completion */
    bool showing_completions;
    char completions[TERM_MAX_COMPLETIONS][256];
    int completion_count;
    int completion_selected;
    int completion_start_col;   /* Column where completion started */
    char completion_prefix[256];
    
    /* Visual state */
    int x, y, w, h;
    term_tab_state_t state;
    bool bell_pending;
    uint32_t bell_time;
    
    /* Cursor blink */
    bool cursor_visible;
    uint32_t last_cursor_blink;
    
    /* Prompt */
    char prompt_format[128];    /* Format string for prompt */
    char current_prompt[256];   /* Current rendered prompt */
    int prompt_len;             /* Visual length of prompt */
    int font_scale;             /* Font scale factor */
    
    /* Process output buffer */
    char output_buffer[8192];
    int output_len;
} term_tab_t;

/* Terminal Window State */
typedef struct {
    window_t *window;
    int width, height;
    
    /* Tabs */
    term_tab_t tabs[TERM_MAX_TABS];
    int tab_count;
    int active_tab;
    int hovered_tab;
    int new_tab_btn_x;
    int tabs_start_x;
    float tab_anim_progress;
    int animating_tab;
    
    /* Content area */
    int content_x, content_y, content_w, content_h;
    int rows, cols;
    int char_w, char_h;
    
    /* Scrollbar */
    bool scrollbar_hovered;
    bool scrollbar_dragging;
    int scrollbar_thumb_y;
    int scrollbar_thumb_h;
    
    /* Input state */
    int drag_tab;
    int drag_start_x;
    int drag_start_tab_x;
    
    /* Animation */
    uint32_t last_frame_time;
    
    /* Global settings */
    char default_cwd[256];
    char default_prompt[128];
    int font_scale;
} terminal_state_t;

static terminal_state_t term = {0};

/* Forward declarations */
static void term_tab_update_prompt(term_tab_t *tab);

/* Built-in commands */
static const char *builtin_names[CMD_BUILTIN_COUNT] = {
    "cd", "ls", "pwd", "echo", "clear", "history", "exit", "help",
    "cat", "mkdir", "rmdir", "rm", "cp", "mv", "touch", "ps", "kill", "jobs"
};

static const char *builtin_help[CMD_BUILTIN_COUNT] = {
    "cd [dir] - Change directory",
    "ls [path] - List directory contents",
    "pwd - Print working directory",
    "echo [args...] - Print arguments",
    "clear - Clear terminal screen",
    "history [n] - Show command history",
    "exit - Exit terminal",
    "help [cmd] - Show help",
    "cat <file> - Display file contents",
    "mkdir <dir> - Create directory",
    "rmdir <dir> - Remove empty directory",
    "rm <file> - Remove file",
    "cp <src> <dst> - Copy file",
    "mv <src> <dst> - Move/rename file",
    "touch <file> - Create empty file",
    "ps - List processes",
    "kill <pid> - Kill process",
    "jobs - List background jobs"
};

/* Font rendering helpers */
static void draw_char_scaled(int x, int y, char c, uint32_t color, int scale) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) uc = '?';
    const uint8_t *glyph = font_8x8[uc - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale == 1) {
                    fb_draw_pixel(x + col, y + row, color);
                } else {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            fb_draw_pixel(x + col * scale + sx, y + row * scale + sy, color);
                        }
                    }
                }
            }
        }
    }
}

static void draw_string_scaled(int x, int y, const char *str, uint32_t color, int scale) {
    int cx = x;
    while (*str) {
        draw_char_scaled(cx, y, *str, color, scale);
        cx += 9 * scale;
        str++;
    }
}

static int str_width_scaled(const char *str, int scale) {
    int w = 0;
    while (*str) { w += 9 * scale; str++; }
    return w;
}

static void draw_string_ellipsis(int x, int y, int max_w, const char *str, uint32_t color, int scale) {
    int len = strlen(str);
    int full_w = str_width_scaled(str, scale);
    if (full_w <= max_w) {
        draw_string_scaled(x, y, str, color, scale);
        return;
    }
    
    char buf[128];
    int left = 0, right = len;
    while (left < right) {
        int mid = (left + right + 1) / 2;
        strncpy(buf, str, mid);
        buf[mid] = '\0';
        if (str_width_scaled(buf, scale) <= max_w - 18) {
            left = mid;
        } else {
            right = mid - 1;
        }
    }
    if (left > 0) {
        strncpy(buf, str, left);
        buf[left] = '\0';
        strcat(buf, "...");
        draw_string_scaled(x, y, buf, color, scale);
    }
}

/* Drawing primitives */
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    fb_info_t *fb = fb_get_info();
    if (x + w > (int)fb->width) w = fb->width - x;
    if (y + h > (int)fb->height) h = fb->height - y;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_draw_pixel(x + i, y + j, color);
        }
    }
}

static void fill_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    fill_rect(x + r, y, w - 2 * r, h, color);
    fill_rect(x, y + r, r, h - 2 * r, color);
    fill_rect(x + w - r, y + r, r, h - 2 * r, color);
    
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

static void draw_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
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

static void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y1;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        fb_draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static int term_sprintf(char *buf, const char *fmt, ...) {
    char *p = buf;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    char *s = __builtin_va_arg(args, char *);
                    if (!s) s = "(null)";
                    while (*s) *p++ = *s++;
                    break;
                }
                case 'd': {
                    int v = __builtin_va_arg(args, int);
                    if (v < 0) { *p++ = '-'; v = -v; }
                    char nbuf[16];
                    int ni = 0;
                    if (v == 0) { *p++ = '0'; break; }
                    while (v) { nbuf[ni++] = '0' + (v % 10); v /= 10; }
                    while (ni--) *p++ = nbuf[ni];
                    break;
                }
                case 'c': *p++ = __builtin_va_arg(args, int); break;
                case '%': *p++ = '%'; break;
                default: *p++ = '%'; *p++ = *fmt; break;
            }
        } else {
            *p++ = *fmt;
        }
        fmt++;
    }
    __builtin_va_end(args);
    *p = '\0';
    return p - buf;
}

static int term_atoi(const char *str) {
    int result = 0;
    int sign = 1;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

static void draw_shadow(int x, int y, int w, int h, int r, int depth) {
    for (int d = 1; d <= depth; d++) {
        uint8_t alpha = 40 - d * 8;
        if (alpha == 0) continue;
        uint32_t shadow = wm_color_blend(0x000000, 0x161628, alpha);
        draw_round_rect(x + d, y + d, w, h, r, shadow);
    }
}

/* ============================================================
 * Terminal Tab Management
 * ============================================================ */

static void term_tab_init(term_tab_t *tab, int id, const char *cwd) {
    memset(tab, 0, sizeof(term_tab_t));
    tab->id = id;
    tab->scrollback_capacity = TERM_SCROLLBACK_LINES;
    tab->scrollback = (term_line_t*)kmalloc(sizeof(term_line_t) * tab->scrollback_capacity);
    if (tab->scrollback) {
        memset(tab->scrollback, 0, sizeof(term_line_t) * tab->scrollback_capacity);
    }
    
    if (cwd) {
        strncpy(tab->cwd, cwd, sizeof(tab->cwd) - 1);
    } else {
        strcpy(tab->cwd, term.default_cwd);
    }
    
    strncpy(tab->prompt_format, term.default_prompt, sizeof(tab->prompt_format) - 1);
    if (tab->prompt_format[0] == '\0') {
        strcpy(tab->prompt_format, "%u@%h:%p$ ");
    }
    
    tab->state = TAB_STATE_NORMAL;
    tab->cursor_visible = true;
    tab->last_cursor_blink = timer_get_ticks();
    tab->font_scale = term.font_scale;
    
    term_tab_update_prompt(tab);
}

static void term_tab_free(term_tab_t *tab) {
    if (tab->scrollback) {
        for (int i = 0; i < tab->scrollback_capacity; i++) {
            if (tab->scrollback[i].colors) {
                kfree(tab->scrollback[i].colors);
            }
        }
        kfree(tab->scrollback);
        tab->scrollback = NULL;
    }
    for (int i = 0; i < tab->history_count; i++) {
        if (tab->history[i]) {
            kfree(tab->history[i]);
        }
    }
}

static void term_tab_add_line(term_tab_t *tab, const char *text, uint32_t color) {
    if (!tab->scrollback) return;
    
    int idx = tab->scrollback_end;
    term_line_t *line = &tab->scrollback[idx];
    
    strncpy(line->text, text, TERM_MAX_LINE_LEN - 1);
    line->text[TERM_MAX_LINE_LEN - 1] = '\0';
    line->len = strlen(line->text);
    line->wrapped = false;
    
    if (color != COL_OUTPUT_STDOUT) {
        line->colors = (uint32_t*)kmalloc(line->len * sizeof(uint32_t));
        if (line->colors) {
            for (int i = 0; i < line->len; i++) {
                line->colors[i] = color;
            }
        }
    }
    
    tab->scrollback_end = (tab->scrollback_end + 1) % tab->scrollback_capacity;
    if (tab->scrollback_end == tab->scrollback_start) {
        /* Buffer full, advance start */
        if (tab->scrollback[tab->scrollback_start].colors) {
            kfree(tab->scrollback[tab->scrollback_start].colors);
        }
        tab->scrollback_start = (tab->scrollback_start + 1) % tab->scrollback_capacity;
    } else {
        tab->scrollback_size++;
    }
    
    /* Reset scroll offset when new output arrives (auto-scroll) */
    if (tab->scroll_offset == 0) {
        tab->scroll_offset = 0;
    }
}

static void term_tab_clear(term_tab_t *tab) {
    if (!tab->scrollback) return;
    for (int i = 0; i < tab->scrollback_capacity; i++) {
        if (tab->scrollback[i].colors) {
            kfree(tab->scrollback[i].colors);
            tab->scrollback[i].colors = NULL;
        }
    }
    tab->scrollback_start = 0;
    tab->scrollback_end = 0;
    tab->scrollback_size = 0;
    tab->scroll_offset = 0;
}

static void term_tab_update_prompt(term_tab_t *tab) {
    char buf[256];
    char *p = buf;
    const char *fmt = tab->prompt_format;
    char cwd_short[256];
    
    /* Get short cwd (last component or ~ for home) */
    const char *home = "/home/user";
    if (strstr(tab->cwd, home) == tab->cwd) {
        term_sprintf(cwd_short, "~%s", tab->cwd + strlen(home));
    } else {
        const char *last_slash = tab->cwd + strlen(tab->cwd) - 1;
        while (last_slash >= tab->cwd && *last_slash != '/') last_slash--;
        if (last_slash >= tab->cwd && last_slash[1]) {
            strcpy(cwd_short, last_slash + 1);
        } else {
            strcpy(cwd_short, tab->cwd);
        }
    }
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'u': p += term_sprintf(p, "user"); break;
                case 'h': p += term_sprintf(p, "myos"); break;
                case 'p': p += term_sprintf(p, "%s", cwd_short); break;
                case 'w': p += term_sprintf(p, "%s", tab->cwd); break;
                case '$': p += term_sprintf(p, "$"); break;
                case '#': p += term_sprintf(p, "#"); break;
                default: *p++ = '%'; *p++ = *fmt; break;
            }
        } else {
            *p++ = *fmt;
        }
        fmt++;
    }
    *p = '\0';
    strcpy(tab->current_prompt, buf);
    tab->prompt_len = strlen(buf);
}

static void term_tab_add_history(term_tab_t *tab, const char *cmd) {
    if (!cmd || !*cmd) return;
    
    /* Don't add duplicates */
    if (tab->history_count > 0 && tab->history[tab->history_count - 1] &&
        strcmp(tab->history[tab->history_count - 1], cmd) == 0) {
        return;
    }
    
    if (tab->history_count >= TERM_HISTORY_SIZE) {
        kfree(tab->history[0]);
        for (int i = 1; i < TERM_HISTORY_SIZE; i++) {
            tab->history[i - 1] = tab->history[i];
        }
        tab->history_count--;
    }
    
    int len = strlen(cmd);
    tab->history[tab->history_count] = (char*)kmalloc(len + 1);
    if (tab->history[tab->history_count]) {
        strcpy(tab->history[tab->history_count], cmd);
    }
    tab->history_count++;
    tab->history_pos = tab->history_count;
}

static void term_tab_history_up(term_tab_t *tab) {
    if (tab->history_count == 0) return;
    if (tab->history_pos > 0) {
        tab->history_pos--;
        strcpy(tab->input_buffer, tab->history[tab->history_pos]);
        tab->input_len = strlen(tab->input_buffer);
        tab->cursor_pos = tab->input_len;
    }
}

static void term_tab_history_down(term_tab_t *tab) {
    if (tab->history_count == 0) return;
    if (tab->history_pos < tab->history_count - 1) {
        tab->history_pos++;
        strcpy(tab->input_buffer, tab->history[tab->history_pos]);
        tab->input_len = strlen(tab->input_buffer);
        tab->cursor_pos = tab->input_len;
    } else if (tab->history_pos == tab->history_count - 1) {
        tab->history_pos = tab->history_count;
        tab->input_buffer[0] = '\0';
        tab->input_len = 0;
        tab->cursor_pos = 0;
    }
}

/* ============================================================
 * Completion System
 * ============================================================ */

static void term_tab_compute_completions(term_tab_t *tab) {
    tab->completion_count = 0;
    tab->completion_selected = 0;
    tab->showing_completions = false;
    
    /* Find the word being completed */
    int end = tab->cursor_pos;
    int start = end;
    while (start > 0 && tab->input_buffer[start - 1] != ' ' && 
           tab->input_buffer[start - 1] != '\t' &&
           tab->input_buffer[start - 1] != '|' &&
           tab->input_buffer[start - 1] != '&' &&
           tab->input_buffer[start - 1] != ';' &&
           tab->input_buffer[start - 1] != '<' &&
           tab->input_buffer[start - 1] != '>') {
        start--;
    }
    
    int word_len = end - start;
    if (word_len <= 0) return;
    
    strncpy(tab->completion_prefix, tab->input_buffer + start, word_len);
    tab->completion_prefix[word_len] = '\0';
    tab->completion_start_col = start;
    
    /* Complete built-ins */
    for (int i = 0; i < CMD_BUILTIN_COUNT && tab->completion_count < TERM_MAX_COMPLETIONS; i++) {
        if (strncmp(builtin_names[i], tab->completion_prefix, word_len) == 0) {
            strcpy(tab->completions[tab->completion_count], builtin_names[i]);
            tab->completion_count++;
        }
    }
    
    /* Complete files in current directory (simplified) */
    /* In a real implementation, this would read the filesystem */
    if (tab->completion_count == 0) {
        /* Add some common paths for demo */
        const char *common_paths[] = {"/home/user/", "/etc/", "/tmp/", "/dev/", "/bin/", "/usr/"};
        for (int i = 0; i < 6 && tab->completion_count < TERM_MAX_COMPLETIONS; i++) {
            if (strncmp(common_paths[i], tab->completion_prefix, word_len) == 0) {
                strcpy(tab->completions[tab->completion_count], common_paths[i]);
                tab->completion_count++;
            }
        }
    }
    
    if (tab->completion_count > 0) {
        tab->showing_completions = true;
    }
}

static void term_tab_apply_completion(term_tab_t *tab) {
    if (!tab->showing_completions || tab->completion_count == 0) return;
    
    const char *completion = tab->completions[tab->completion_selected];
    int prefix_len = strlen(tab->completion_prefix);
    int completion_len = strlen(completion);
    
    if (completion_len <= prefix_len) return;
    
    /* Insert the rest of the completion */
    int insert_len = completion_len - prefix_len;
    if (tab->input_len + insert_len >= TERM_MAX_LINE_LEN - 1) return;
    
    memmove(&tab->input_buffer[tab->cursor_pos + insert_len],
            &tab->input_buffer[tab->cursor_pos],
            tab->input_len - tab->cursor_pos + 1);
    memcpy(&tab->input_buffer[tab->cursor_pos], completion + prefix_len, insert_len);
    tab->input_len += insert_len;
    tab->cursor_pos += insert_len;
    
    tab->showing_completions = false;
}

static void term_tab_next_completion(term_tab_t *tab) {
    if (!tab->showing_completions || tab->completion_count == 0) return;
    tab->completion_selected = (tab->completion_selected + 1) % tab->completion_count;
}

static void term_tab_prev_completion(term_tab_t *tab) {
    if (!tab->showing_completions || tab->completion_count == 0) return;
    tab->completion_selected = (tab->completion_selected - 1 + tab->completion_count) % tab->completion_count;
}

/* ============================================================
 * Input Handling
 * ============================================================ */

static void term_tab_insert_char(term_tab_t *tab, char c) {
    if (tab->input_len >= TERM_MAX_LINE_LEN - 1) return;
    if (tab->showing_completions) {
        term_tab_apply_completion(tab);
    }
    memmove(&tab->input_buffer[tab->cursor_pos + 1],
            &tab->input_buffer[tab->cursor_pos],
            tab->input_len - tab->cursor_pos + 1);
    tab->input_buffer[tab->cursor_pos] = c;
    tab->input_len++;
    tab->cursor_pos++;
    tab->history_pos = tab->history_count;
}

static void term_tab_backspace(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    if (tab->cursor_pos > 0) {
        memmove(&tab->input_buffer[tab->cursor_pos - 1],
                &tab->input_buffer[tab->cursor_pos],
                tab->input_len - tab->cursor_pos + 1);
        tab->input_len--;
        tab->cursor_pos--;
        tab->history_pos = tab->history_count;
    }
}

static void term_tab_delete(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    if (tab->cursor_pos < tab->input_len) {
        memmove(&tab->input_buffer[tab->cursor_pos],
                &tab->input_buffer[tab->cursor_pos + 1],
                tab->input_len - tab->cursor_pos);
        tab->input_len--;
        tab->history_pos = tab->history_count;
    }
}

static void term_tab_move_cursor_left(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    if (tab->cursor_pos > 0) tab->cursor_pos--;
}

static void term_tab_move_cursor_right(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    if (tab->cursor_pos < tab->input_len) tab->cursor_pos++;
}

static void term_tab_move_cursor_home(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    tab->cursor_pos = 0;
}

static void term_tab_move_cursor_end(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    tab->cursor_pos = tab->input_len;
}

static void term_tab_move_cursor_word_left(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    while (tab->cursor_pos > 0 && tab->input_buffer[tab->cursor_pos - 1] == ' ') {
        tab->cursor_pos--;
    }
    while (tab->cursor_pos > 0 && tab->input_buffer[tab->cursor_pos - 1] != ' ') {
        tab->cursor_pos--;
    }
}

static void term_tab_move_cursor_word_right(term_tab_t *tab) {
    if (tab->showing_completions) {
        tab->showing_completions = false;
        return;
    }
    while (tab->cursor_pos < tab->input_len && tab->input_buffer[tab->cursor_pos] == ' ') {
        tab->cursor_pos++;
    }
    while (tab->cursor_pos < tab->input_len && tab->input_buffer[tab->cursor_pos] != ' ') {
        tab->cursor_pos++;
    }
}

/* ============================================================
 * Command Execution
 * ============================================================ */

static void term_execute_builtin(term_tab_t *tab, char *cmdline) {
    char *argv[64];
    int argc = 0;
    char *p = cmdline;
    
    while (*p && argc < 63) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = NULL;
    
    if (argc == 0) return;
    
    const char *cmd = argv[0];
    
    if (strcmp(cmd, "cd") == 0) {
        const char *target = argc > 1 ? argv[1] : term.default_cwd;
        if (target[0] == '~') {
            char expanded[256];
            term_sprintf(expanded, "%s%s", term.default_cwd, target + 1);
            target = expanded;
        }
        /* In real implementation, would chdir and update cwd */
        term_sprintf(tab->cwd, "%s", target);
        term_tab_update_prompt(tab);
        return;
    }
    
    if (strcmp(cmd, "pwd") == 0) {
        term_tab_add_line(tab, tab->cwd, COL_OUTPUT_STDOUT);
        return;
    }
    
    if (strcmp(cmd, "echo") == 0) {
        char output[1024] = {0};
        for (int i = 1; i < argc; i++) {
            strcat(output, argv[i]);
            if (i < argc - 1) strcat(output, " ");
        }
        term_tab_add_line(tab, output, COL_OUTPUT_STDOUT);
        return;
    }
    
    if (strcmp(cmd, "clear") == 0) {
        term_tab_clear(tab);
        return;
    }
    
    if (strcmp(cmd, "history") == 0) {
        int n = argc > 1 ? term_atoi(argv[1]) : tab->history_count;
        if (n > tab->history_count) n = tab->history_count;
        for (int i = tab->history_count - n; i < tab->history_count; i++) {
            if (tab->history[i]) {
                char line[256];
                term_sprintf(line, "  %d  %s", i + 1, tab->history[i]);
                term_tab_add_line(tab, line, COL_OUTPUT_STDOUT);
            }
        }
        return;
    }
    
    if (strcmp(cmd, "exit") == 0) {
        term_tab_add_line(tab, "exit", COL_OUTPUT_STDOUT);
        /* In real implementation, would close tab */
        return;
    }
    
    if (strcmp(cmd, "help") == 0) {
        if (argc > 1) {
            for (int i = 0; i < CMD_BUILTIN_COUNT; i++) {
                if (strcmp(argv[1], builtin_names[i]) == 0) {
                    char line[256];
                    term_sprintf(line, "%s - %s", builtin_names[i], builtin_help[i]);
                    term_tab_add_line(tab, line, COL_OUTPUT_STDOUT);
                    return;
                }
            }
            char line[256];
            term_sprintf(line, "help: no help for '%s'", argv[1]);
            term_tab_add_line(tab, line, COL_OUTPUT_STDERR);
        } else {
            term_tab_add_line(tab, "Built-in commands:", COL_CMD_BUILTIN);
            for (int i = 0; i < CMD_BUILTIN_COUNT; i++) {
                char line[256];
                term_sprintf(line, "  %-12s %s", builtin_names[i], builtin_help[i]);
                term_tab_add_line(tab, line, COL_OUTPUT_STDOUT);
            }
        }
        return;
    }
    
    if (strcmp(cmd, "ls") == 0) {
        term_tab_add_line(tab, "Desktop    Documents  Downloads  Music  Pictures  Videos", COL_OUTPUT_STDOUT);
        term_tab_add_line(tab, "file.txt   readme.md  config.ini  data/", COL_OUTPUT_STDOUT);
        return;
    }
    
    if (strcmp(cmd, "cat") == 0) {
        if (argc < 2) {
            term_tab_add_line(tab, "cat: missing file operand", COL_OUTPUT_STDERR);
            return;
        }
        char line[256];
        term_sprintf(line, "cat: %s: No such file or directory", argv[1]);
        term_tab_add_line(tab, line, COL_OUTPUT_STDERR);
        return;
    }
    
    if (strcmp(cmd, "ps") == 0) {
        term_tab_add_line(tab, "  PID TTY          TIME CMD", COL_OUTPUT_STDOUT);
        term_tab_add_line(tab, "    1 ?        00:00:00 init", COL_OUTPUT_STDOUT);
        term_tab_add_line(tab, "    2 ?        00:00:00 kthreadd", COL_OUTPUT_STDOUT);
        term_tab_add_line(tab, "  100 ?        00:00:00 terminal", COL_OUTPUT_STDOUT);
        term_tab_add_line(tab, "  101 ?        00:00:00 browser", COL_OUTPUT_STDOUT);
        return;
    }
    
    /* Unknown builtin - treat as external command */
    char line[512];
    term_sprintf(line, "%s: command not found", cmd);
    term_tab_add_line(tab, line, COL_OUTPUT_STDERR);
}

static void term_tab_execute_command(term_tab_t *tab) {
    if (tab->input_len == 0) {
        term_tab_add_line(tab, "", COL_OUTPUT_STDOUT);
        term_tab_update_prompt(tab);
        return;
    }
    
    /* Add to history */
    term_tab_add_history(tab, tab->input_buffer);
    
    /* Echo command with prompt */
    char echo_line[TERM_MAX_LINE_LEN + 256];
    term_sprintf(echo_line, "%s%s", tab->current_prompt, tab->input_buffer);
    term_tab_add_line(tab, echo_line, COL_OUTPUT_STDOUT);
    
    /* Execute */
    term_execute_builtin(tab, tab->input_buffer);
    
    /* Clear input */
    tab->input_buffer[0] = '\0';
    tab->input_len = 0;
    tab->cursor_pos = 0;
    tab->history_pos = tab->history_count;
    tab->showing_completions = false;
    
    /* Update prompt (in case cwd changed) */
    term_tab_update_prompt(tab);
}

/* ============================================================
 * Selection and Clipboard
 * ============================================================ */

static void term_tab_start_selection(term_tab_t *tab, int x, int y) {
    tab->selecting = true;
    tab->has_selection = false;
    tab->sel_start_x = tab->sel_end_x = x;
    tab->sel_start_y = tab->sel_end_y = y;
}

static void term_tab_update_selection(term_tab_t *tab, int x, int y) {
    if (!tab->selecting) return;
    tab->sel_end_x = x;
    tab->sel_end_y = y;
    tab->has_selection = (tab->sel_start_x != tab->sel_end_x || tab->sel_start_y != tab->sel_end_y);
}

static void term_tab_end_selection(term_tab_t *tab) {
    tab->selecting = false;
}

static void term_tab_copy_selection(term_tab_t *tab) {
    if (!tab->has_selection) return;
    /* In real implementation, would copy to clipboard */
    /* For now, just indicate copy succeeded */
}

static void term_tab_paste(term_tab_t *tab) {
    /* In real implementation, would paste from clipboard */
    /* For demo, paste a sample string */
    const char *clipboard = "echo \"pasted from clipboard\"\n";
    int len = strlen(clipboard);
    if (tab->input_len + len < TERM_MAX_LINE_LEN - 1) {
        memcpy(&tab->input_buffer[tab->cursor_pos], clipboard, len);
        tab->input_len += len;
        tab->cursor_pos += len;
    }
}

/* ============================================================
 * Scrollback
 * ============================================================ */

static void term_tab_scroll_up(term_tab_t *tab, int lines) {
    int max_scroll = tab->scrollback_size - term.rows + 1;
    if (max_scroll < 0) max_scroll = 0;
    tab->scroll_offset += lines;
    if (tab->scroll_offset > max_scroll) tab->scroll_offset = max_scroll;
}

static void term_tab_scroll_down(term_tab_t *tab, int lines) {
    tab->scroll_offset -= lines;
    if (tab->scroll_offset < 0) tab->scroll_offset = 0;
}

static void term_tab_scroll_page_up(term_tab_t *tab) {
    term_tab_scroll_up(tab, term.rows - 1);
}

static void term_tab_scroll_page_down(term_tab_t *tab) {
    term_tab_scroll_down(tab, term.rows - 1);
}

/* ============================================================
 * Tab Management
 * ============================================================ */

static void term_add_tab(void) {
    if (term.tab_count >= TERM_MAX_TABS) return;
    
    int new_id = term.tab_count;
    term_tab_init(&term.tabs[new_id], new_id, term.default_cwd);
    strcpy(term.tabs[new_id].title, "Terminal");
    term.tab_count++;
    term.active_tab = new_id;
    term.tab_anim_progress = 0.0f;
    term.animating_tab = new_id;
}

static void term_close_tab(int index) {
    if (term.tab_count <= 1) return;
    if (index < 0 || index >= term.tab_count) return;
    
    term_tab_free(&term.tabs[index]);
    
    for (int i = index; i < term.tab_count - 1; i++) {
        term.tabs[i] = term.tabs[i + 1];
    }
    term.tab_count--;
    
    if (term.active_tab >= term.tab_count) {
        term.active_tab = term.tab_count - 1;
    }
}

static void term_switch_tab(int index) {
    if (index < 0 || index >= term.tab_count) return;
    term.active_tab = index;
}

static void term_next_tab(void) {
    if (term.tab_count <= 1) return;
    term.active_tab = (term.active_tab + 1) % term.tab_count;
}

static void term_prev_tab(void) {
    if (term.tab_count <= 1) return;
    term.active_tab = (term.active_tab - 1 + term.tab_count) % term.tab_count;
}

/* ============================================================
 * Window Initialization
 * ============================================================ */

/* ============================================================
 * Window-Manager-driven interface
 * The WM calls these; the terminal is drawn on-demand and is
 * event-driven instead of running its own polling loop.
 * ============================================================ */

/* Native handlers (defined below) take window-local coordinates. */
static void term_draw(void);
static void term_update(void);
static void term_handle_key_down(int key);
static void term_handle_key_up(int key);
static void term_handle_mouse_move(int x, int y);
static void term_handle_mouse_down(int x, int y, int button);
static void term_handle_mouse_up(int x, int y, int button);
static void term_handle_mouse_wheel(int delta);

static void term_wm_draw(struct window *win) {
    (void)win;
    term_update();
    term_draw();
}

static void term_wm_key_down(struct window *win, int key) {
    (void)win;
    term_handle_key_down(key);
}

static void term_wm_key_up(struct window *win, int key) {
    (void)win;
    term_handle_key_up(key);
}

static void term_wm_mouse_down(struct window *win, int x, int y, int button) {
    (void)win;
    term_handle_mouse_down(x, y, button);
}

static void term_wm_mouse_up(struct window *win, int x, int y, int button) {
    (void)win;
    term_handle_mouse_up(x, y, button);
}

static void term_wm_mouse_move(struct window *win, int x, int y) {
    (void)win;
    term_handle_mouse_move(x, y);
}

static void term_wm_mouse_wheel(struct window *win, int delta) {
    (void)win;
    term_handle_mouse_wheel(delta);
}

static void term_init_window(void) {
    fb_info_t *fb = fb_get_info();
    int win_w = 900;
    int win_h = 600;
    int win_x = (fb->width - win_w) / 2;
    int win_y = (fb->height - win_h) / 2;
    
    term.window = wm_get_window(wm_create_window("Terminal", win_x, win_y, win_w, win_h, 0));
    if (!term.window) return;
    wm_focus_window(term.window->id);
    
    term.width = win_w;
    term.height = win_h;
    term.window->user_data = &term;
    
    term.window->draw_content = term_wm_draw;
    term.window->on_mouse_down = term_wm_mouse_down;
    term.window->on_mouse_up = term_wm_mouse_up;
    term.window->on_mouse_move = term_wm_mouse_move;
    term.window->on_mouse_wheel = term_wm_mouse_wheel;
    term.window->on_key_down = term_wm_key_down;
    term.window->on_key_up = term_wm_key_up;
}

static void term_init_tabs(void) {
    term.tab_count = 1;
    term.active_tab = 0;
    term.hovered_tab = -1;
    term.tabs_start_x = 8;
    
    term_tab_init(&term.tabs[0], 0, term.default_cwd);
    strcpy(term.tabs[0].title, "Terminal");
    term.tabs[0].state = TAB_STATE_ACTIVE;
    
    term.tab_anim_progress = 1.0f;
    term.animating_tab = -1;
}

static void term_calc_content_area(void) {
    term.content_x = 0;
    term.content_y = TERM_TAB_HEIGHT;
    term.content_w = term.width;
    term.content_h = term.height - TERM_TAB_HEIGHT;
    
    term.char_w = 9 * term.font_scale;
    term.char_h = 16 * term.font_scale;
    term.cols = term.content_w / term.char_w;
    term.rows = term.content_h / term.char_h;
}

static void term_init(void) {
    memset(&term, 0, sizeof(terminal_state_t));
    
    strcpy(term.default_cwd, "/home/user");
    strcpy(term.default_prompt, "%u@%h:%p$ ");
    term.font_scale = 1;
    
    term_init_window();
    term_init_tabs();
    term_calc_content_area();
    
    term.drag_tab = -1;
    term.last_frame_time = timer_get_ticks();
}

/* ============================================================
 * Drawing Functions
 * ============================================================ */

static void term_draw_tab_bar(void) {
    int tab_y = 0;
    int tab_h = TERM_TAB_HEIGHT;
    int x = term.tabs_start_x;
    
    /* Tab bar background with acrylic effect */
    fill_rect(0, tab_y, term.width, tab_h, COL_TAB_BAR_BG);
    draw_line(0, tab_y + tab_h - 1, term.width, tab_y + tab_h - 1, COL_TAB_BORDER);
    
    /* Draw tabs */
    for (int i = 0; i < term.tab_count; i++) {
        term_tab_t *tab = &term.tabs[i];
        bool active = (i == term.active_tab);
        bool hovered = (i == term.hovered_tab);
        
        int tab_w = TERM_TAB_MAX_WIDTH;
        int title_w = str_width_scaled(tab->title, 1) + 36;
        if (title_w < TERM_TAB_MIN_WIDTH) title_w = TERM_TAB_MIN_WIDTH;
        if (title_w < tab_w) tab_w = title_w;
        
        tab->x = x;
        tab->y = tab_y;
        tab->w = tab_w;
        tab->h = tab_h;
        
        uint32_t bg_color;
        uint32_t text_color;
        if (active) {
            bg_color = COL_TAB_ACTIVE_BG;
            text_color = COL_TAB_TEXT_ACTIVE;
        } else if (hovered) {
            bg_color = COL_TAB_HOVER_BG;
            text_color = COL_TAB_TEXT_ACTIVE;
        } else {
            bg_color = COL_TAB_INACTIVE_BG;
            text_color = COL_TAB_TEXT_INACTIVE;
        }
        
        if (tab->state == TAB_STATE_BELL) {
            bg_color = 0x4A2A2A;
            text_color = 0xFF8888;
        }
        
        /* Tab background with rounded top corners */
        fill_round_rect(x, tab_y, tab_w, tab_h, 6, bg_color);
        
        /* Active tab indicator */
        if (active) {
            fill_rect(x + 4, tab_y + tab_h - 2, tab_w - 8, 2, COL_ACCENT_BLUE);
        }
        
        /* Tab border */
        draw_round_rect(x, tab_y, tab_w, tab_h, 6, COL_TAB_BORDER);
        if (active) {
            draw_line(x + 6, tab_y + tab_h - 1, x + tab_w - 6, tab_y + tab_h - 1, bg_color);
        }
        
        /* Bell indicator */
        if (tab->state == TAB_STATE_BELL) {
            int bell_x = x + tab_w - 10;
            int bell_y = tab_y + 4;
            draw_string_scaled(bell_x, bell_y, "!", 0xFF6666, 1);
        }
        
        /* Title */
        int title_x = x + 10;
        int title_max_w = tab_w - 30;
        if (active) title_max_w -= 20;
        draw_string_ellipsis(title_x, tab_y + (tab_h - 10) / 2, title_max_w, tab->title, text_color, 1);
        
        /* Close button on active tab */
        if (active) {
            int close_x = x + tab_w - 20;
            int close_y = tab_y + (tab_h - 14) / 2;
            bool close_hover = hovered && 
                mouse_get_state().x >= term.window->x + close_x && 
                mouse_get_state().x < term.window->x + close_x + 14 &&
                mouse_get_state().y >= term.window->y + close_y && 
                mouse_get_state().y < term.window->y + close_y + 14;
            
            if (close_hover) {
                fill_round_rect(close_x, close_y, 14, 14, 3, 0x333355);
            }
            draw_line(close_x + 3, close_y + 3, close_x + 11, close_y + 11, COL_TEXT_MUTED);
            draw_line(close_x + 11, close_y + 3, close_x + 3, close_y + 11, COL_TEXT_MUTED);
        }
        
        x += tab_w + 2;
    }
    
    /* New tab button */
    term.new_tab_btn_x = x;
    int btn_y = tab_y + (tab_h - 22) / 2;
    bool new_tab_hover = (term.hovered_tab == -2);
    
    fill_round_rect(x, btn_y, TERM_NEW_TAB_BTN_W, 22, 5, new_tab_hover ? COL_TAB_HOVER_BG : COL_TAB_INACTIVE_BG);
    draw_round_rect(x, btn_y, TERM_NEW_TAB_BTN_W, 22, 5, COL_TAB_BORDER);
    
    /* Plus icon */
    int cx = x + TERM_NEW_TAB_BTN_W / 2;
    int cy = btn_y + 11;
    draw_line(cx - 5, cy, cx + 5, cy, new_tab_hover ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
    draw_line(cx, cy - 5, cx, cy + 5, new_tab_hover ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
}

static void term_draw_content(void) {
    term_tab_t *tab = &term.tabs[term.active_tab];
    if (!tab) return;
    
    int cx = term.content_x;
    int cy = term.content_y;
    int cw = term.content_w;
    int ch = term.content_h;
    
    /* Background */
    fill_rect(cx, cy, cw, ch, COL_TERM_BG);
    
    /* Calculate visible lines */
    int visible_rows = term.rows - 1; /* Leave one row for input */
    int start_idx;
    
    if (tab->scroll_offset > 0) {
        /* Scrolled up - show older lines */
        int end_pos = (tab->scrollback_end - 1 - tab->scroll_offset + tab->scrollback_capacity) % tab->scrollback_capacity;
        start_idx = (end_pos - visible_rows + 1 + tab->scrollback_capacity) % tab->scrollback_capacity;
    } else {
        /* At bottom - show newest lines */
        start_idx = (tab->scrollback_end - visible_rows + tab->scrollback_capacity) % tab->scrollback_capacity;
        if (tab->scrollback_size < visible_rows) {
            start_idx = tab->scrollback_start;
        }
    }
    
    /* Draw scrollback lines */
    int line_y = cy;
    int idx = start_idx;
    int drawn = 0;
    
    while (drawn < visible_rows && tab->scrollback_size > 0) {
        term_line_t *line = &tab->scrollback[idx];
        if (line->len > 0 || line->wrapped) {
            uint32_t default_color = COL_OUTPUT_STDOUT;
            for (int i = 0; i < line->len; i++) {
                uint32_t color = line->colors ? line->colors[i] : default_color;
                draw_char_scaled(cx + 8 + i * term.char_w, line_y + 2, line->text[i], color, term.font_scale);
            }
        }
        line_y += term.char_h;
        drawn++;
        idx = (idx + 1) % tab->scrollback_capacity;
        if (idx == tab->scrollback_end) break;
    }
    
    /* Draw input line at bottom */
    int input_y = cy + ch - term.char_h - 2;
    
    /* Draw prompt with colors */
    int px = cx + 8;
    int py = input_y + 2;
    const char *prompt = tab->current_prompt;
    int prompt_drawn = 0;
    
    while (*prompt) {
        uint32_t color = COL_PROMPT_SYMBOL;
        if (prompt_drawn < 4) color = COL_PROMPT_USER;      /* user */
        else if (prompt_drawn < 9) color = COL_PROMPT_HOST;  /* @myos */
        else if (prompt_drawn < 10) color = COL_PROMPT_SYMBOL; /* : */
        else color = COL_PROMPT_PATH;                        /* path */
        if (*prompt == '$' || *prompt == '#') color = COL_PROMPT_SYMBOL;
        draw_char_scaled(px, py, *prompt, color, term.font_scale);
        px += term.char_w;
        prompt++;
        prompt_drawn++;
    }
    
    /* Draw input buffer */
    for (int i = 0; i < tab->input_len; i++) {
        draw_char_scaled(px, py, tab->input_buffer[i], COL_TEXT_PRIMARY, term.font_scale);
        px += term.char_w;
    }
    
    /* Draw cursor */
    uint32_t now = timer_get_ticks();
    if (now - tab->last_cursor_blink > TERM_CURSOR_BLINK_MS) {
        tab->cursor_visible = !tab->cursor_visible;
        tab->last_cursor_blink = now;
    }
    
    if (tab->cursor_visible) {
        int cursor_x = cx + 8 + tab->prompt_len * term.char_w + tab->cursor_pos * term.char_w;
        fill_rect(cursor_x, py, term.char_w, term.char_h, COL_CURSOR);
    }
    
    /* Draw selection */
    if (tab->has_selection) {
        /* Simplified selection highlight */
        int sel_x1 = cx + 8 + tab->sel_start_x * term.char_w;
        int sel_y1 = cy + tab->sel_start_y * term.char_h;
        int sel_x2 = cx + 8 + tab->sel_end_x * term.char_w;
        int sel_y2 = cy + tab->sel_end_y * term.char_h;
        
        if (sel_y1 == sel_y2) {
            if (sel_x1 > sel_x2) { int tmp = sel_x1; sel_x1 = sel_x2; sel_x2 = tmp; }
            fill_rect(sel_x1, sel_y1, sel_x2 - sel_x1, term.char_h, COL_SELECTION_BG);
        }
    }
    
    /* Draw completions popup */
    if (tab->showing_completions && tab->completion_count > 0) {
        int popup_x = cx + 8 + tab->completion_start_col * term.char_w;
        int popup_y = input_y - TERM_COMPLETION_POPUP_H;
        if (popup_y < cy) popup_y = input_y + term.char_h + 2;
        int popup_w = 300;
        int popup_h = tab->completion_count * (term.char_h + 2) + 8;
        if (popup_h > TERM_COMPLETION_POPUP_H) popup_h = TERM_COMPLETION_POPUP_H;
        
        draw_shadow(popup_x - 2, popup_y - 2, popup_w + 4, popup_h + 4, 6, 4);
        fill_round_rect(popup_x, popup_y, popup_w, popup_h, 6, 0x1E1E3A);
        draw_round_rect(popup_x, popup_y, popup_w, popup_h, 6, COL_ACCENT_BLUE);
        
        int item_y = popup_y + 4;
        for (int i = 0; i < tab->completion_count && item_y < popup_y + popup_h - 4; i++) {
            uint32_t bg = (i == tab->completion_selected) ? COL_SELECTION_BG : 0x1E1E3A;
            uint32_t fg = (i == tab->completion_selected) ? COL_SELECTION_TEXT : COL_TEXT_PRIMARY;
            
            fill_rect(popup_x + 2, item_y, popup_w - 4, term.char_h + 2, bg);
            draw_string_scaled(popup_x + 6, item_y + 2, tab->completions[i], fg, term.font_scale);
            item_y += term.char_h + 2;
        }
    }
    
    /* Draw scrollbar */
    if (tab->scrollback_size > visible_rows) {
        int sb_x = cx + cw - 10;
        int sb_y = cy;
        int sb_w = 6;
        int sb_h = ch;
        
        fill_rect(sb_x, sb_y, sb_w, sb_h, COL_SCROLLBAR_BG);
        
        float scroll_ratio = (float)visible_rows / tab->scrollback_size;
        int thumb_h = (int)(sb_h * scroll_ratio);
        if (thumb_h < 30) thumb_h = 30;
        
        int max_scroll = tab->scrollback_size - visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        int thumb_y = sb_y + (int)((float)tab->scroll_offset / max_scroll * (sb_h - thumb_h));
        
        uint32_t thumb_color = term.scrollbar_hovered ? COL_SCROLLBAR_HOVER : COL_SCROLLBAR_THUMB;
        fill_round_rect(sb_x + 1, thumb_y, sb_w - 2, thumb_h, 2, thumb_color);
        
        term.scrollbar_thumb_y = thumb_y;
        term.scrollbar_thumb_h = thumb_h;
    }
}

static void term_draw(void) {
    if (!term.window) return;
    
    term_draw_tab_bar();
    term_draw_content();
}

/* ============================================================
 * Input Handling
 * ============================================================ */

static term_tab_t* term_get_active_tab(void) {
    if (term.active_tab < 0 || term.active_tab >= term.tab_count) return NULL;
    return &term.tabs[term.active_tab];
}

static void term_handle_mouse_move(int x, int y) {
    mouse_state_t mouse = mouse_get_state();
    int wx = x;
    int wy = y;
    
    /* Tab bar hover */
    term.hovered_tab = -1;
    if (wy >= 0 && wy < TERM_TAB_HEIGHT) {
        int tx = term.tabs_start_x;
        for (int i = 0; i < term.tab_count; i++) {
            term_tab_t *tab = &term.tabs[i];
            if (wx >= tab->x && wx < tab->x + tab->w) {
                term.hovered_tab = i;
                break;
            }
            tx += tab->w + 2;
        }
        if (wx >= term.new_tab_btn_x && wx < term.new_tab_btn_x + TERM_NEW_TAB_BTN_W &&
            wy >= (TERM_TAB_HEIGHT - 22) / 2 && wy < (TERM_TAB_HEIGHT + 22) / 2) {
            term.hovered_tab = -2;
        }
    }
    
    /* Tab dragging */
    if (term.drag_tab >= 0 && (mouse.buttons & 0x01)) {
        int delta = wx - term.drag_start_x;
        term.tabs[term.drag_tab].x = term.drag_start_tab_x + delta;
    }
    
    /* Scrollbar hover */
    term_tab_t *tab = term_get_active_tab();
    if (tab && tab->scrollback_size > term.rows - 1) {
        int sb_x = term.content_x + term.content_w - 10;
        int sb_y = term.content_y;
        int sb_w = 6;
        int sb_h = term.content_h;
        
        term.scrollbar_hovered = (wx >= sb_x && wx < sb_x + sb_w && wy >= sb_y && wy < sb_y + sb_h);
        
        if (term.scrollbar_dragging && (mouse.buttons & 0x01)) {
            int rel_y = wy - sb_y - term.scrollbar_thumb_h / 2;
            float ratio = (float)rel_y / (sb_h - term.scrollbar_thumb_h);
            int max_scroll = tab->scrollback_size - term.rows + 1;
            if (max_scroll < 0) max_scroll = 0;
            tab->scroll_offset = (int)(ratio * max_scroll);
            if (tab->scroll_offset < 0) tab->scroll_offset = 0;
            if (tab->scroll_offset > max_scroll) tab->scroll_offset = max_scroll;
        }
    }
    
    /* Selection drag */
    if (tab && tab->selecting && (mouse.buttons & 0x01)) {
        int rel_x = (wx - term.content_x - 8) / term.char_w;
        int rel_y = (wy - term.content_y) / term.char_h;
        term_tab_update_selection(tab, rel_x, rel_y);
    }
}

static void term_handle_mouse_down(int x, int y, int button) {
    if (button != 1) return;
    
    int wx = x;
    int wy = y;
    
    /* Tab clicks */
    if (wy >= 0 && wy < TERM_TAB_HEIGHT) {
        int tx = term.tabs_start_x;
        for (int i = 0; i < term.tab_count; i++) {
            term_tab_t *tab = &term.tabs[i];
            if (wx >= tab->x && wx < tab->x + tab->w) {
                if (i == term.active_tab) {
                    int close_x = tab->x + tab->w - 20;
                    int close_y = (TERM_TAB_HEIGHT - 14) / 2;
                    if (wx >= close_x && wx < close_x + 14 && wy >= close_y && wy < close_y + 14) {
                        term_close_tab(i);
                        return;
                    }
                }
                
                term.drag_tab = i;
                term.drag_start_x = wx;
                term.drag_start_tab_x = tab->x;
                
                if (i != term.active_tab) {
                    term_switch_tab(i);
                }
                return;
            }
            tx += tab->w + 2;
        }
        
        if (wx >= term.new_tab_btn_x && wx < term.new_tab_btn_x + TERM_NEW_TAB_BTN_W &&
            wy >= (TERM_TAB_HEIGHT - 22) / 2 && wy < (TERM_TAB_HEIGHT + 22) / 2) {
            term_add_tab();
            return;
        }
    }
    
    /* Content area clicks */
    term_tab_t *tab = term_get_active_tab();
    if (tab && wy >= term.content_y) {
        int rel_x = (wx - term.content_x - 8) / term.char_w;
        int rel_y = (wy - term.content_y) / term.char_h;
        
        /* Scrollbar click */
        if (tab->scrollback_size > term.rows - 1) {
            int sb_x = term.content_x + term.content_w - 10;
            int sb_y = term.content_y;
            int sb_w = 6;
            int sb_h = term.content_h;
            
            if (wx >= sb_x && wx < sb_x + sb_w && wy >= sb_y && wy < sb_y + sb_h) {
                if (wy >= term.scrollbar_thumb_y && wy < term.scrollbar_thumb_y + term.scrollbar_thumb_h) {
                    term.scrollbar_dragging = true;
                } else {
                    /* Click on track - jump */
                    float ratio = (float)(wy - sb_y) / sb_h;
                    int max_scroll = tab->scrollback_size - term.rows + 1;
                    if (max_scroll < 0) max_scroll = 0;
                    tab->scroll_offset = (int)(ratio * max_scroll);
                    if (tab->scroll_offset < 0) tab->scroll_offset = 0;
                    if (tab->scroll_offset > max_scroll) tab->scroll_offset = max_scroll;
                }
                return;
            }
        }
        
        /* Double-click for word selection */
        static uint32_t last_click_time = 0;
        static int last_click_x = -1, last_click_y = -1;
        uint32_t now = timer_get_ticks();
        
        if (now - last_click_time < 300 && wx == last_click_x && wy == last_click_y) {
            /* Double-click - select word (simplified) */
            term_tab_start_selection(tab, rel_x, rel_y);
            term_tab_update_selection(tab, rel_x + 1, rel_y);
            term_tab_end_selection(tab);
            last_click_time = 0;
            return;
        }
        last_click_time = now;
        last_click_x = wx;
        last_click_y = wy;
        
        /* Start selection */
        term_tab_start_selection(tab, rel_x, rel_y);
        
        /* Focus input if clicking on input line */
        int input_y = term.content_y + term.content_h - term.char_h - 2;
        if (wy >= input_y && wy < input_y + term.char_h) {
            tab->cursor_pos = tab->input_len;
        }
    }
}

static void term_handle_mouse_up(int x, int y, int button) {
    (void)x; (void)y;
    if (button != 1) return;
    
    term.drag_tab = -1;
    term.scrollbar_dragging = false;
    
    term_tab_t *tab = term_get_active_tab();
    if (tab) {
        term_tab_end_selection(tab);
    }
}

static void term_handle_mouse_wheel(int delta) {
    term_tab_t *tab = term_get_active_tab();
    if (!tab) return;
    
    if (delta > 0) {
        term_tab_scroll_up(tab, 3);
    } else {
        term_tab_scroll_down(tab, 3);
    }
}

static void term_handle_key_down(int key) {
    term_tab_t *tab = term_get_active_tab();
    if (!tab) return;
    
    uint8_t mods = keyboard_get_modifiers();
    bool ctrl = mods & KMOD_CTRL;
    bool shift = mods & KMOD_SHIFT;
    bool alt = mods & KMOD_ALT;
    
    /* Handle completions first */
    if (tab->showing_completions) {
        switch (key) {
            case KEY_TAB:
                if (shift) term_tab_prev_completion(tab);
                else term_tab_next_completion(tab);
                return;
            case KEY_ENTER:
                term_tab_apply_completion(tab);
                return;
            case KEY_ESCAPE:
                tab->showing_completions = false;
                return;
            case KEY_UP:
                term_tab_prev_completion(tab);
                return;
            case KEY_DOWN:
                term_tab_next_completion(tab);
                return;
        }
    }
    
    /* Global shortcuts */
    if (ctrl && shift) {
        switch (key) {
            case KEY_C:
                term_tab_copy_selection(tab);
                return;
            case KEY_V:
                term_tab_paste(tab);
                return;
            case KEY_T:
                /* Reopen closed tab - not implemented */
                return;
            case KEY_N:
                term_add_tab();
                return;
            case KEY_W:
                term_close_tab(term.active_tab);
                return;
            case KEY_PAGE_UP:
                term_tab_scroll_page_up(tab);
                return;
            case KEY_PAGE_DOWN:
                term_tab_scroll_page_down(tab);
                return;
        }
    }
    
    if (ctrl && !shift) {
        switch (key) {
            case KEY_C:
                /* Ctrl+C - send interrupt */
                term_tab_add_line(tab, "^C", COL_OUTPUT_STDERR);
                tab->input_buffer[0] = '\0';
                tab->input_len = 0;
                tab->cursor_pos = 0;
                tab->history_pos = tab->history_count;
                return;
            case KEY_V:
                term_tab_paste(tab);
                return;
            case KEY_L:
                term_tab_clear(tab);
                return;
            case KEY_T:
                term_add_tab();
                return;
            case KEY_W:
                term_close_tab(term.active_tab);
                return;
            case KEY_TAB:
                term_next_tab();
                return;
            case KEY_LEFT:
                term_prev_tab();
                return;
            case KEY_RIGHT:
                term_next_tab();
                return;
            case KEY_A:
                term_tab_move_cursor_home(tab);
                return;
            case KEY_E:
                term_tab_move_cursor_end(tab);
                return;
            case KEY_U:
                /* Clear line */
                tab->input_buffer[0] = '\0';
                tab->input_len = 0;
                tab->cursor_pos = 0;
                return;
            case KEY_K:
                /* Kill to end of line */
                tab->input_buffer[tab->cursor_pos] = '\0';
                tab->input_len = tab->cursor_pos;
                return;
        }
    }
    
    if (alt && !ctrl) {
        switch (key) {
            case KEY_LEFT:
                term_tab_move_cursor_word_left(tab);
                return;
            case KEY_RIGHT:
                term_tab_move_cursor_word_right(tab);
                return;
            case KEY_B:
                term_tab_move_cursor_word_left(tab);
                return;
            case KEY_F:
                term_tab_move_cursor_word_right(tab);
                return;
        }
    }
    
    switch (key) {
        case KEY_ENTER:
            term_tab_execute_command(tab);
            return;
        case KEY_TAB:
            term_tab_compute_completions(tab);
            return;
        case KEY_BACKSPACE:
            term_tab_backspace(tab);
            return;
        case KEY_DELETE:
            term_tab_delete(tab);
            return;
        case KEY_LEFT:
            term_tab_move_cursor_left(tab);
            return;
        case KEY_RIGHT:
            term_tab_move_cursor_right(tab);
            return;
        case KEY_UP:
            term_tab_history_up(tab);
            return;
        case KEY_DOWN:
            term_tab_history_down(tab);
            return;
        case KEY_HOME:
            term_tab_move_cursor_home(tab);
            return;
        case KEY_END:
            term_tab_move_cursor_end(tab);
            return;
        case KEY_PAGE_UP:
            term_tab_scroll_page_up(tab);
            return;
        case KEY_PAGE_DOWN:
            term_tab_scroll_page_down(tab);
            return;
        case KEY_ESCAPE:
            if (tab->showing_completions) {
                tab->showing_completions = false;
            } else if (tab->has_selection) {
                tab->has_selection = false;
                tab->selecting = false;
            } else {
                tab->input_buffer[0] = '\0';
                tab->input_len = 0;
                tab->cursor_pos = 0;
                tab->history_pos = tab->history_count;
            }
            return;
        default:
            /* Keycodes are USB-HID usage IDs; map to ASCII for printable
             * characters (modifiers already applied by the keyboard layer). */
            if (!ctrl && !alt) {
                char c = keyboard_keycode_to_ascii((uint16_t)key, keyboard_get_modifiers());
                if (c >= 0x20 && c <= 0x7E && tab->input_len < TERM_MAX_LINE_LEN - 1) {
                    term_tab_insert_char(tab, c);
                }
            }
            return;
    }
}

static void term_handle_key_up(int key) {
    (void)key;
}

static void term_update(void) {
    if (term.animating_tab >= 0 && term.tab_anim_progress < 1.0f) {
        term.tab_anim_progress += 0.15f;
        if (term.tab_anim_progress >= 1.0f) {
            term.tab_anim_progress = 1.0f;
            term.animating_tab = -1;
        }
    }
    
    /* Check for bell timeout */
    for (int i = 0; i < term.tab_count; i++) {
        term_tab_t *tab = &term.tabs[i];
        if (tab->state == TAB_STATE_BELL && timer_get_ticks() - tab->bell_time > 2000) {
            tab->state = TAB_STATE_NORMAL;
        }
    }
    
    term.last_frame_time = timer_get_ticks();
}

/* Forward declaration */
int terminal_main(void);

/* Terminal thread entry point - can be called as a kernel thread */
void terminal_thread_entry(void *arg) {
    (void)arg;
    terminal_main();
}

/* Terminal registration - called at desktop init (and via thread entry).
 * Registers the terminal window with the WM; drawing and input are driven
 * by the WM through the window callbacks, so this returns at once. */
int terminal_main(void) {
    static bool inited = false;
    if (inited && term.window) return 0;
    
    if (!inited) {
        term_init();
        inited = true;
    }
    
    if (!term.window) {
        puts("Failed to create terminal window");
        return 1;
    }
    
    /* Add welcome message */
    term_tab_t *tab = &term.tabs[0];
    term_tab_add_line(tab, "Welcome to MyOS Terminal", COL_CMD_BUILTIN);
    term_tab_add_line(tab, "Type 'help' for available commands", COL_TEXT_SECONDARY);
    term_tab_add_line(tab, "", COL_OUTPUT_STDOUT);
    
    return 0;
}