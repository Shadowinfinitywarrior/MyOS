#include "libc.h"
#include "wm.h"
#include "../drivers/framebuffer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../kernel/timer.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* Browser Configuration */
#define BROWSER_MAX_TABS          16
#define BROWSER_TAB_HEIGHT        36
#define BROWSER_TOOLBAR_HEIGHT    44
#define BROWSER_BOOKMARKS_HEIGHT  32
#define BROWSER_TAB_MIN_WIDTH     120
#define BROWSER_TAB_MAX_WIDTH     240
#define BROWSER_NEW_TAB_BTN_W     32
#define BROWSER_URL_BAR_HEIGHT    32
#define BROWSER_NAV_BTN_SIZE      28

/* Color Scheme - Windows 11 Dark Mode */
#define COL_BG                    0x1A1A1A
#define COL_TAB_BAR_BG            0x1F1F1F
#define COL_TAB_ACTIVE_BG         0x252525
#define COL_TAB_INACTIVE_BG       0x1F1F1F
#define COL_TAB_HOVER_BG          0x2A2A2A
#define COL_TAB_BORDER            0x333333
#define COL_TOOLBAR_BG            0x1F1F1F
#define COL_URL_BAR_BG            0x252525
#define COL_URL_BAR_BORDER        0x3A3A3A
#define COL_URL_BAR_FOCUS         0x00A4EF
#define COL_URL_BAR_TEXT          0xFFFFFF
#define COL_URL_BAR_PLACEHOLDER   0x888888
#define COL_NAV_BTN_BG            0x252525
#define COL_NAV_BTN_HOVER         0x2D2D2D
#define COL_NAV_BTN_PRESSED       0x333333
#define COL_NAV_BTN_ICON          0xCCCCCC
#define COL_BOOKMARKS_BG          0x1F1F1F
#define COL_BOOKMARK_ITEM_HOVER   0x2A2A2A
#define COL_BOOKMARK_TEXT         0xCCCCCC
#define COL_PAGE_BG               0xFFFFFF
#define COL_PAGE_TEXT             0x1A1A1A
#define COL_LINK                  0x0060A0
#define COL_ACCENT_BLUE           0x00A4EF
#define COL_ACCENT_HOVER          0x0078D7
#define COL_INCOGNITO_PURPLE      0x6C4FA0
#define COL_INCOGNITO_BG          0x1A1420
#define COL_SCROLLBAR_BG          0x1A1A1A
#define COL_SCROLLBAR_THUMB       0x444444
#define COL_TEXT_PRIMARY          0xFFFFFF
#define COL_TEXT_SECONDARY        0xAAAAAA
#define COL_TEXT_MUTED            0x666666
#define COL_SEPARATOR             0x333333
#define COL_SHADOW                0x000000

/* Tab States */
typedef enum {
    TAB_STATE_NORMAL = 0,
    TAB_STATE_HOVERED,
    TAB_STATE_ACTIVE,
    TAB_STATE_LOADING,
    TAB_STATE_INCOGNITO
} tab_state_t;

/* Navigation Button Types */
typedef enum {
    NAV_BTN_BACK = 0,
    NAV_BTN_FORWARD,
    NAV_BTN_RELOAD,
    NAV_BTN_HOME,
    NAV_BTN_COUNT
} nav_btn_t;

/* Tab Structure */
typedef struct browser_tab {
    int id;
    char title[128];
    char url[512];
    bool incognito;
    bool loading;
    float load_progress;
    int x, y, w, h;
    tab_state_t state;
    int favicon_color;
    bool pinned;
} browser_tab_t;

/* Bookmark Structure */
typedef struct bookmark {
    char name[64];
    char url[256];
    uint32_t color;
} bookmark_t;

/* Navigation Button Structure */
typedef struct nav_button {
    int x, y, w, h;
    nav_btn_t type;
    bool hovered;
    bool pressed;
    bool enabled;
    char *icon;
} nav_button_t;

/* Browser Window State */
typedef struct {
    window_t *window;
    int width, height;
    
    /* Tabs */
    browser_tab_t tabs[BROWSER_MAX_TABS];
    int tab_count;
    int active_tab;
    int hovered_tab;
    int new_tab_btn_x;
    int tabs_start_x;
    float tab_anim_progress;
    int animating_tab;
    
    /* Toolbar */
    nav_button_t nav_buttons[NAV_BTN_COUNT];
    int url_bar_x, url_bar_y, url_bar_w, url_bar_h;
    char url_buffer[512];
    int url_cursor_pos;
    bool url_focused;
    bool url_select_all;
    char search_engine[64];
    
    /* Bookmarks bar */
    bool show_bookmarks;
    bookmark_t bookmarks[16];
    int bookmark_count;
    int hovered_bookmark;
    
    /* Page content area */
    int content_x, content_y, content_w, content_h;
    int scroll_y;
    int max_scroll_y;
    
    /* Incognito mode */
    bool incognito_mode;
    
    /* Download manager */
    bool show_downloads;
    char download_items[8][128];
    int download_count;
    float download_progress[8];
    
    /* Input state */
    int drag_tab;
    int drag_start_x;
    int drag_start_tab_x;
    
    /* Animation */
    uint32_t last_frame_time;
} browser_state_t;

static browser_state_t browser = {0};

/* Font rendering helpers */
static void draw_char_scaled(int x, int y, char c, uint32_t color, int scale) {
    extern const uint8_t font_8x8[96][8];
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
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
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

static void draw_triangle(int cx, int cy, int size, uint32_t color, bool right) {
    if (right) {
        for (int i = 0; i < size; i++) {
            draw_line(cx - size/2 + i, cy - i, cx - size/2 + i, cy + i, color);
        }
    } else {
        for (int i = 0; i < size; i++) {
            draw_line(cx + size/2 - i, cy - i, cx + size/2 - i, cy + i, color);
        }
    }
}

static float cosf(float x) {
    x = x - (int)(x / (2 * 3.14159f)) * 2 * 3.14159f;
    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i <= 10; i++) {
        term *= -x * x / (2 * i * (2 * i - 1));
        result += term;
    }
    return result;
}

static float sinf(float x) {
    return cosf(x - 3.14159f / 2);
}

static int browser_sprintf(char *buf, const char *fmt, ...) {
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

static void draw_shadow(int x, int y, int w, int h, int r, int depth) {
    for (int d = 1; d <= depth; d++) {
        uint8_t alpha = 40 - d * 8;
        if (alpha == 0) continue;
        uint32_t shadow = wm_color_blend(0x000000, 0x1C1C1C, alpha);
        draw_round_rect(x + d, y + d, w, h, r, shadow);
    }
}

/* Browser functions */
static void browser_init_window(void) {
    fb_info_t *fb = fb_get_info();
    int win_w = 1024;
    int win_h = 700;
    int win_x = (fb->width - win_w) / 2;
    int win_y = (fb->height - win_h) / 2;
    
    browser.window = wm_get_window(wm_create_window("Browser", win_x, win_y, win_w, win_h, 0));
    if (!browser.window) return;
    
    browser.width = win_w;
    browser.height = win_h;
    browser.window->draw_content = (void (*)(struct window*))0; /* We'll handle drawing in browser_draw */
    browser.window->user_data = &browser;
    
    /* Set up window callbacks */
    browser.window->on_mouse_down = (void (*)(struct window*, int, int, int))0;
    browser.window->on_mouse_up = (void (*)(struct window*, int, int, int))0;
    browser.window->on_mouse_move = (void (*)(struct window*, int, int))0;
    browser.window->on_key_down = (void (*)(struct window*, int))0;
    browser.window->on_key_up = (void (*)(struct window*, int))0;
    browser.window->on_resize = (void (*)(struct window*, int, int))0;
}

static void browser_init_tabs(void) {
    browser.tab_count = 1;
    browser.active_tab = 0;
    browser.hovered_tab = -1;
    browser.tabs_start_x = 8;
    
    browser_tab_t *tab = &browser.tabs[0];
    tab->id = 0;
    strcpy(tab->title, "New Tab");
    strcpy(tab->url, "https://www.bing.com");
    tab->incognito = false;
    tab->loading = false;
    tab->load_progress = 0.0f;
    tab->state = TAB_STATE_ACTIVE;
    tab->favicon_color = COL_ACCENT_BLUE;
    tab->pinned = false;
    
    browser.tab_anim_progress = 1.0f;
    browser.animating_tab = -1;
}

static void browser_init_nav_buttons(void) {
    int btn_y = BROWSER_TAB_HEIGHT + 6;
    int btn_x = 12;
    
    const char *icons[NAV_BTN_COUNT] = {"<", ">", "R", "H"};
    for (int i = 0; i < NAV_BTN_COUNT; i++) {
        nav_button_t *btn = &browser.nav_buttons[i];
        btn->x = btn_x;
        btn->y = btn_y;
        btn->w = BROWSER_NAV_BTN_SIZE;
        btn->h = BROWSER_NAV_BTN_SIZE;
        btn->type = i;
        btn->hovered = false;
        btn->pressed = false;
        btn->enabled = true;
        btn->icon = (char*)icons[i];
        btn_x += BROWSER_NAV_BTN_SIZE + 4;
    }
    
    /* Disable back/forward initially */
    browser.nav_buttons[NAV_BTN_BACK].enabled = false;
    browser.nav_buttons[NAV_BTN_FORWARD].enabled = false;
}

static void browser_init_url_bar(void) {
    browser.url_bar_x = browser.nav_buttons[NAV_BTN_COUNT - 1].x + browser.nav_buttons[NAV_BTN_COUNT - 1].w + 12;
    browser.url_bar_y = BROWSER_TAB_HEIGHT + 6;
    browser.url_bar_h = BROWSER_URL_BAR_HEIGHT;
    browser.url_bar_w = browser.width - browser.url_bar_x - 12;
    
    strcpy(browser.url_buffer, "https://www.bing.com");
    browser.url_cursor_pos = strlen(browser.url_buffer);
    browser.url_focused = false;
    browser.url_select_all = false;
    strcpy(browser.search_engine, "https://www.bing.com/search?q=");
}

static void browser_init_bookmarks(void) {
    browser.show_bookmarks = true;
    browser.bookmark_count = 6;
    browser.hovered_bookmark = -1;
    
    const char *names[] = {"Bing", "GitHub", "YouTube", "Reddit", "Wikipedia", "Gmail"};
    const char *urls[] = {
        "https://www.bing.com",
        "https://github.com",
        "https://www.youtube.com",
        "https://www.reddit.com",
        "https://www.wikipedia.org",
        "https://mail.google.com"
    };
    uint32_t colors[] = {0x00A4EF, 0x333333, 0xFF0000, 0xFF4500, 0x0060A0, 0xEA4335};
    
    for (int i = 0; i < browser.bookmark_count; i++) {
        strcpy(browser.bookmarks[i].name, names[i]);
        strcpy(browser.bookmarks[i].url, urls[i]);
        browser.bookmarks[i].color = colors[i];
    }
}

static void browser_init_downloads(void) {
    browser.show_downloads = false;
    browser.download_count = 0;
    for (int i = 0; i < 8; i++) {
        browser.download_progress[i] = 0.0f;
    }
}

static void browser_init(void) {
    memset(&browser, 0, sizeof(browser_state_t));
    browser_init_window();
    browser_init_tabs();
    browser_init_nav_buttons();
    browser_init_url_bar();
    browser_init_bookmarks();
    browser_init_downloads();
    
    browser.incognito_mode = false;
    browser.scroll_y = 0;
    browser.max_scroll_y = 0;
    browser.drag_tab = -1;
    browser.last_frame_time = timer_get_ticks();
}

static void browser_add_tab(bool incognito) {
    if (browser.tab_count >= BROWSER_MAX_TABS) return;
    
    int new_id = browser.tab_count;
    browser_tab_t *tab = &browser.tabs[new_id];
    tab->id = new_id;
    if (incognito) {
        strcpy(tab->title, "Incognito");
        strcpy(tab->url, "about:blank");
        tab->incognito = true;
        tab->favicon_color = COL_INCOGNITO_PURPLE;
    } else {
        strcpy(tab->title, "New Tab");
        strcpy(tab->url, "https://www.bing.com");
        tab->incognito = false;
        tab->favicon_color = COL_ACCENT_BLUE;
    }
    tab->loading = false;
    tab->load_progress = 0.0f;
    tab->state = TAB_STATE_NORMAL;
    tab->pinned = false;
    
    browser.tab_count++;
    browser.active_tab = new_id;
    browser.tab_anim_progress = 0.0f;
    browser.animating_tab = new_id;
    
    strcpy(browser.url_buffer, tab->url);
    browser.url_cursor_pos = strlen(browser.url_buffer);
}

static void browser_close_tab(int index) {
    if (browser.tab_count <= 1) return;
    if (index < 0 || index >= browser.tab_count) return;
    
    for (int i = index; i < browser.tab_count - 1; i++) {
        browser.tabs[i] = browser.tabs[i + 1];
    }
    browser.tab_count--;
    
    if (browser.active_tab >= browser.tab_count) {
        browser.active_tab = browser.tab_count - 1;
    }
    
    if (browser.active_tab >= 0) {
        strcpy(browser.url_buffer, browser.tabs[browser.active_tab].url);
        browser.url_cursor_pos = strlen(browser.url_buffer);
    }
}

static void browser_switch_tab(int index) {
    if (index < 0 || index >= browser.tab_count) return;
    browser.active_tab = index;
    strcpy(browser.url_buffer, browser.tabs[index].url);
    browser.url_cursor_pos = strlen(browser.url_buffer);
    browser.url_focused = false;
}

static void browser_navigate(const char *url) {
    if (browser.active_tab < 0) return;
    
    browser_tab_t *tab = &browser.tabs[browser.active_tab];
    strncpy(tab->url, url, sizeof(tab->url) - 1);
    tab->loading = true;
    tab->load_progress = 0.0f;
    tab->state = TAB_STATE_LOADING;
    
    /* Extract domain for title */
    const char *domain_start = strstr(url, "://");
    if (domain_start) domain_start += 3;
    else domain_start = url;
    
    const char *domain_end = strchr(domain_start, '/');
    if (!domain_end) domain_end = domain_start + strlen(domain_start);
    
    int len = domain_end - domain_start;
    if (len > 127) len = 127;
    strncpy(tab->title, domain_start, len);
    tab->title[len] = '\0';
    
    strcpy(browser.url_buffer, url);
    browser.url_cursor_pos = strlen(browser.url_buffer);
    
    /* Enable back button */
    browser.nav_buttons[NAV_BTN_BACK].enabled = true;
}

static void browser_go_back(void) {
    /* Simulated - in real browser would go back in history */
    if (browser.active_tab >= 0) {
        browser_tab_t *tab = &browser.tabs[browser.active_tab];
        strcpy(tab->url, "https://www.bing.com");
        strcpy(tab->title, "Bing");
        strcpy(browser.url_buffer, tab->url);
        browser.url_cursor_pos = strlen(browser.url_buffer);
    }
}

static void browser_go_forward(void) {
    /* Simulated */
}

static void browser_reload(void) {
    if (browser.active_tab >= 0) {
        browser_tab_t *tab = &browser.tabs[browser.active_tab];
        tab->loading = true;
        tab->load_progress = 0.0f;
        tab->state = TAB_STATE_LOADING;
    }
}

static void browser_go_home(void) {
    browser_navigate("https://www.bing.com");
}

static bool is_url(const char *str) {
    return strstr(str, ".") != NULL && 
           (strstr(str, "http://") == str || strstr(str, "https://") == str || 
            strstr(str, "www.") == str || strchr(str, '.') != NULL);
}

static void browser_handle_url_enter(void) {
    char url[512];
    strcpy(url, browser.url_buffer);
    
    if (!is_url(url)) {
        /* Treat as search query */
        char search_url[512];
        browser_sprintf(search_url, "%s%s", browser.search_engine, url);
        browser_navigate(search_url);
    } else if (strstr(url, "http://") != url && strstr(url, "https://") != url) {
        char full_url[512];
        browser_sprintf(full_url, "https://%s", url);
        browser_navigate(full_url);
    } else {
        browser_navigate(url);
    }
    browser.url_focused = false;
}

/* Drawing functions */
static void browser_draw_tab_bar(void) {
    int tab_y = 0;
    int tab_h = BROWSER_TAB_HEIGHT;
    int x = browser.tabs_start_x;
    
    /* Tab bar background */
    fill_rect(0, tab_y, browser.width, tab_h, browser.incognito_mode ? COL_INCOGNITO_BG : COL_TAB_BAR_BG);
    
    /* Draw tabs */
    for (int i = 0; i < browser.tab_count; i++) {
        browser_tab_t *tab = &browser.tabs[i];
        bool active = (i == browser.active_tab);
        bool hovered = (i == browser.hovered_tab);
        
        int tab_w = BROWSER_TAB_MAX_WIDTH;
        int title_w = str_width_scaled(tab->title, 1) + 36; /* icon + padding + close */
        if (title_w < BROWSER_TAB_MIN_WIDTH) title_w = BROWSER_TAB_MIN_WIDTH;
        if (title_w < tab_w) tab_w = title_w;
        
        tab->x = x;
        tab->y = tab_y;
        tab->w = tab_w;
        tab->h = tab_h;
        
        uint32_t bg_color;
        if (active) bg_color = tab->incognito ? COL_INCOGNITO_PURPLE : COL_TAB_ACTIVE_BG;
        else if (hovered) bg_color = COL_TAB_HOVER_BG;
        else bg_color = COL_TAB_INACTIVE_BG;
        
        /* Tab background with rounded top corners */
        fill_round_rect(x, tab_y, tab_w, tab_h, 8, bg_color);
        
        /* Active tab indicator */
        if (active) {
            fill_rect(x + 4, tab_y + tab_h - 2, tab_w - 8, 2, COL_ACCENT_BLUE);
        }
        
        /* Tab border */
        draw_round_rect(x, tab_y, tab_w, tab_h, 8, COL_TAB_BORDER);
        /* Bottom border not drawn for active tab */
        if (active) {
            draw_line(x + 8, tab_y + tab_h - 1, x + tab_w - 8, tab_y + tab_h - 1, bg_color);
        }
        
        /* Favicon */
        int icon_x = x + 10;
        int icon_y = tab_y + (tab_h - 16) / 2;
        fill_round_rect(icon_x, icon_y, 16, 16, 3, tab->favicon_color);
        
        /* Title */
        int title_x = x + 30;
        int title_max_w = tab_w - 40;
        if (active && !tab->pinned) title_max_w -= 20; /* Space for close button */
        draw_string_ellipsis(title_x, tab_y + (tab_h - 10) / 2, title_max_w, tab->title, 
                            active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY, 1);
        
        /* Close button on active tab */
        if (active && !tab->pinned) {
            int close_x = x + tab_w - 22;
            int close_y = tab_y + (tab_h - 16) / 2;
            bool close_hover = hovered && mouse_get_state().x >= close_x && mouse_get_state().x < close_x + 16 &&
                              mouse_get_state().y >= close_y && mouse_get_state().y < close_y + 16;
            
            if (close_hover) {
                fill_round_rect(close_x, close_y, 16, 16, 4, 0x333333);
            }
            draw_line(close_x + 4, close_y + 4, close_x + 12, close_y + 12, COL_TEXT_SECONDARY);
            draw_line(close_x + 12, close_y + 4, close_x + 4, close_y + 12, COL_TEXT_SECONDARY);
        }
        
        /* Loading indicator */
        if (tab->loading) {
            int prog_w = (int)((tab_w - 8) * tab->load_progress);
            if (prog_w > 0) {
                fill_rect(x + 4, tab_y + tab_h - 2, prog_w, 2, COL_ACCENT_BLUE);
            }
            /* Spinner */
            uint32_t now = timer_get_ticks();
            int spin_angle = (now / 50) % 360;
            int cx = x + tab_w - 14;
            int cy = tab_y + tab_h / 2;
            for (int a = 0; a < 360; a += 45) {
                float rad = (a + spin_angle) * 3.14159f / 180.0f;
                int sx = cx + (int)(6 * cosf(rad));
                int sy = cy + (int)(6 * sinf(rad));
                uint32_t c = (a == 0) ? COL_ACCENT_BLUE : COL_TEXT_MUTED;
                fb_draw_pixel(sx, sy, c);
            }
        }
        
        x += tab_w + 2;
    }
    
    /* New tab button */
    browser.new_tab_btn_x = x;
    int btn_y = tab_y + (tab_h - 24) / 2;
    bool new_tab_hover = (browser.hovered_tab == -2);
    
    fill_round_rect(x, btn_y, BROWSER_NEW_TAB_BTN_W, 24, 6, new_tab_hover ? COL_TAB_HOVER_BG : COL_TAB_INACTIVE_BG);
    draw_round_rect(x, btn_y, BROWSER_NEW_TAB_BTN_W, 24, 6, COL_TAB_BORDER);
    
    /* Plus icon */
    int cx = x + BROWSER_NEW_TAB_BTN_W / 2;
    int cy = btn_y + 12;
    draw_line(cx - 6, cy, cx + 6, cy, new_tab_hover ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
    draw_line(cx, cy - 6, cx, cy + 6, new_tab_hover ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
    
    /* Incognito indicator */
    if (browser.incognito_mode) {
        int incog_x = browser.width - 120;
        fill_round_rect(incog_x, tab_y + 4, 112, tab_h - 8, 12, COL_INCOGNITO_PURPLE);
        draw_string_scaled(incog_x + 8, tab_y + 8, "InPrivate", 0xFFFFFF, 1);
        /* Hat icon */
        draw_triangle(incog_x + 100, tab_y + tab_h/2, 8, 0xFFFFFF, false);
    }
}

static void browser_draw_nav_buttons(void) {
    for (int i = 0; i < NAV_BTN_COUNT; i++) {
        nav_button_t *btn = &browser.nav_buttons[i];
        
        uint32_t bg = btn->enabled ? (btn->pressed ? COL_NAV_BTN_PRESSED : (btn->hovered ? COL_NAV_BTN_HOVER : COL_NAV_BTN_BG)) 
                                   : COL_NAV_BTN_BG;
        uint32_t icon_color = btn->enabled ? (btn->hovered ? COL_TEXT_PRIMARY : COL_NAV_BTN_ICON) : COL_TEXT_MUTED;
        
        fill_round_rect(btn->x, btn->y, btn->w, btn->h, 6, bg);
        if (btn->hovered && btn->enabled) {
            draw_round_rect(btn->x, btn->y, btn->w, btn->h, 6, COL_SEPARATOR);
        }
        
        int icon_x = btn->x + (btn->w - 9) / 2;
        int icon_y = btn->y + (btn->h - 10) / 2;
        draw_string_scaled(icon_x, icon_y, btn->icon, icon_color, 1);
    }
}

static void browser_draw_url_bar(void) {
    bool focused = browser.url_focused;
    uint32_t border_color = focused ? COL_URL_BAR_FOCUS : COL_URL_BAR_BORDER;
    uint32_t bg_color = COL_URL_BAR_BG;
    
    /* Background */
    fill_round_rect(browser.url_bar_x, browser.url_bar_y, browser.url_bar_w, browser.url_bar_h, 6, bg_color);
    draw_round_rect(browser.url_bar_x, browser.url_bar_y, browser.url_bar_w, browser.url_bar_h, 6, border_color);
    if (focused) {
        draw_round_rect(browser.url_bar_x - 1, browser.url_bar_y - 1, browser.url_bar_w + 2, browser.url_bar_h + 2, 7, COL_URL_BAR_FOCUS);
    }
    
    /* Lock icon for HTTPS */
    bool secure = strstr(browser.url_buffer, "https://") == browser.url_buffer;
    int lock_x = browser.url_bar_x + 10;
    int lock_y = browser.url_bar_y + (browser.url_bar_h - 16) / 2;
    if (secure) {
        fill_round_rect(lock_x, lock_y + 4, 10, 10, 2, COL_ACCENT_BLUE);
        fill_rect(lock_x + 2, lock_y, 6, 4, COL_ACCENT_BLUE);
    } else {
        draw_string_scaled(lock_x, lock_y, "!", COL_TEXT_MUTED, 1);
    }
    
    /* URL text */
    int text_x = browser.url_bar_x + 26;
    int text_max_w = browser.url_bar_w - 36;
    int text_y = browser.url_bar_y + (browser.url_bar_h - 10) / 2;
    
    if (browser.url_focused && browser.url_select_all) {
        fill_rect(text_x, text_y - 2, text_max_w, 14, COL_URL_BAR_FOCUS);
        draw_string_scaled(text_x + 4, text_y, browser.url_buffer, 0xFFFFFF, 1);
    } else {
        draw_string_ellipsis(text_x + 4, text_y, text_max_w - 8, browser.url_buffer, 
                            browser.url_focused ? COL_URL_BAR_TEXT : COL_URL_BAR_PLACEHOLDER, 1);
    }
    
    /* Cursor when focused */
    if (browser.url_focused && !browser.url_select_all && (timer_get_ticks() / 500) % 2 == 0) {
        int cursor_x = text_x + 4 + str_width_scaled(browser.url_buffer, 1);
        if (cursor_x < text_x + text_max_w) {
            draw_line(cursor_x, text_y, cursor_x, text_y + 10, COL_URL_BAR_FOCUS);
        }
    }
    
    /* Go/Refresh button at end of URL bar */
    int go_x = browser.url_bar_x + browser.url_bar_w - 36;
    int go_y = browser.url_bar_y + (browser.url_bar_h - 24) / 2;
    fill_round_rect(go_x, go_y, 28, 24, 4, focused ? COL_URL_BAR_FOCUS : COL_NAV_BTN_BG);
    draw_string_scaled(go_x + 8, go_y + 4, browser.tabs[browser.active_tab].loading ? "X" : ">", 
                      focused ? 0xFFFFFF : COL_NAV_BTN_ICON, 1);
}

static void browser_draw_bookmarks_bar(void) {
    if (!browser.show_bookmarks) return;
    
    int bar_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT;
    int bar_h = BROWSER_BOOKMARKS_HEIGHT;
    
    fill_rect(0, bar_y, browser.width, bar_h, COL_BOOKMARKS_BG);
    draw_line(0, bar_y + bar_h - 1, browser.width, bar_y + bar_h - 1, COL_SEPARATOR);
    
    int x = 12;
    for (int i = 0; i < browser.bookmark_count; i++) {
        bookmark_t *bm = &browser.bookmarks[i];
        bool hovered = (i == browser.hovered_bookmark);
        
        int name_w = str_width_scaled(bm->name, 1);
        int item_w = name_w + 20 + 24; /* icon + padding */
        
        if (hovered) {
            fill_round_rect(x, bar_y + 4, item_w, bar_h - 8, 4, COL_BOOKMARK_ITEM_HOVER);
        }
        
        /* Favicon */
        fill_round_rect(x + 4, bar_y + 6, 16, 16, 3, bm->color);
        
        /* Name */
        draw_string_scaled(x + 24, bar_y + (bar_h - 10) / 2, bm->name, COL_BOOKMARK_TEXT, 1);
        
        x += item_w + 4;
    }
    
    /* Add bookmark button */
    int add_x = x;
    fill_round_rect(add_x, bar_y + 4, 24, bar_h - 8, 4, COL_NAV_BTN_BG);
    draw_string_scaled(add_x + 6, bar_y + 8, "+", COL_TEXT_SECONDARY, 1);
}

static void browser_draw_page_content(void) {
    int content_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT + (browser.show_bookmarks ? BROWSER_BOOKMARKS_HEIGHT : 0);
    browser.content_x = 0;
    browser.content_y = content_y;
    browser.content_w = browser.width;
    browser.content_h = browser.height - content_y - WM_TASKBAR_HEIGHT;
    
    browser_tab_t *tab = &browser.tabs[browser.active_tab];
    uint32_t page_bg = tab->incognito ? COL_INCOGNITO_BG : COL_PAGE_BG;
    uint32_t page_text = tab->incognito ? 0xDDDDDD : COL_PAGE_TEXT;
    
    fill_rect(browser.content_x, browser.content_y, browser.content_w, browser.content_h, page_bg);
    
    if (tab->loading) {
        /* Loading spinner */
        int cx = browser.content_x + browser.content_w / 2;
        int cy = browser.content_y + browser.content_h / 2;
        uint32_t now = timer_get_ticks();
        int spin_angle = (now / 50) % 360;
        
        for (int a = 0; a < 360; a += 30) {
            float rad = (a + spin_angle) * 3.14159f / 180.0f;
            int sx = cx + (int)(20 * cosf(rad));
            int sy = cy + (int)(20 * sinf(rad));
            uint32_t c = (a == 0) ? COL_ACCENT_BLUE : COL_TEXT_MUTED;
            fill_rect(sx - 2, sy - 2, 4, 4, c);
        }
        
        char loading_text[64];
        browser_sprintf(loading_text, "Loading... %d%%", (int)(tab->load_progress * 100));
        int tw = str_width_scaled(loading_text, 1);
        draw_string_scaled(cx - tw/2, cy + 30, loading_text, COL_TEXT_SECONDARY, 1);
        
        /* Simulate loading progress */
        tab->load_progress += 0.02f;
        if (tab->load_progress >= 1.0f) {
            tab->loading = false;
            tab->load_progress = 1.0f;
            tab->state = tab->incognito ? TAB_STATE_INCOGNITO : TAB_STATE_ACTIVE;
        }
        return;
    }
    
    /* Render page content (placeholder) */
    int text_x = browser.content_x + 40;
    int text_y = browser.content_y + 40 - browser.scroll_y;
    
    if (tab->incognito) {
        /* Incognito page */
        draw_string_scaled(text_x, text_y, "Incognito Mode", COL_INCOGNITO_PURPLE, 2);
        text_y += 40;
        draw_string_scaled(text_x, text_y, "You've gone incognito.", page_text, 1);
        text_y += 20;
        draw_string_scaled(text_x, text_y, "Pages you view in this window won't appear in", page_text, 1);
        text_y += 16;
        draw_string_scaled(text_x, text_y, "your browser history, cookie store, or search", page_text, 1);
        text_y += 16;
        draw_string_scaled(text_x, text_y, "history after you close all incognito windows.", page_text, 1);
        text_y += 30;
        draw_string_scaled(text_x, text_y, "However, downloads and bookmarks will be saved.", page_text, 1);
        text_y += 30;
        draw_string_scaled(text_x, text_y, "Learn more about Incognito mode", COL_LINK, 1);
    } else if (strcmp(tab->url, "https://www.bing.com") == 0 || strcmp(tab->url, "about:blank") == 0) {
        /* New tab page */
        draw_string_scaled(text_x, text_y, "New Tab", page_text, 2);
        text_y += 40;
        
        /* Search box */
        int search_w = 500;
        int search_h = 44;
        int search_x = text_x;
        int search_y = text_y;
        fill_round_rect(search_x, search_y, search_w, search_h, 22, 0xF0F0F0);
        draw_round_rect(search_x, search_y, search_w, search_h, 22, 0xCCCCCC);
        draw_string_scaled(search_x + 20, search_y + 14, "Search the web", 0x999999, 1);
        
        text_y += search_h + 30;
        
        /* Quick links */
        draw_string_scaled(text_x, text_y, "Quick links", page_text, 1);
        text_y += 24;
        
        const char *quick_names[] = {"Bing", "YouTube", "Facebook", "Amazon", "Wikipedia", "Reddit"};
        const char *quick_urls[] = {
            "https://www.bing.com", "https://www.youtube.com", "https://www.facebook.com",
            "https://www.amazon.com", "https://www.wikipedia.org", "https://www.reddit.com"
        };
        (void)quick_urls;
        
        for (int i = 0; i < 6; i++) {
            int link_x = text_x + (i % 3) * 180;
            int link_y = text_y + (i / 3) * 80;
            
            fill_round_rect(link_x, link_y, 160, 64, 8, 0xF5F5F5);
            draw_round_rect(link_x, link_y, 160, 64, 8, 0xE0E0E0);
            
            int tw = str_width_scaled(quick_names[i], 1);
            draw_string_scaled(link_x + (160 - tw) / 2, link_y + 24, quick_names[i], page_text, 1);
        }
        
        browser.max_scroll_y = text_y + 180 - browser.content_y;
    } else {
        /* Generic page content */
        draw_string_scaled(text_x, text_y, tab->title, page_text, 2);
        text_y += 40;
        draw_string_scaled(text_x, text_y, tab->url, COL_LINK, 1);
        text_y += 30;
        
        draw_string_scaled(text_x, text_y, "This is a demo browser page.", page_text, 1);
        text_y += 20;
        draw_string_scaled(text_x, text_y, "In a real implementation, this would render HTML/CSS content.", page_text, 1);
        text_y += 20;
        draw_string_scaled(text_x, text_y, "Features demonstrated:", page_text, 1);
        text_y += 20;
        
        const char *features[] = {
            "- Tabbed browsing with rounded tabs",
            "- Address bar with search integration",
            "- Navigation buttons (back, forward, reload, home)",
            "- Bookmarks bar",
            "- Incognito/InPrivate mode",
            "- Windows 11 dark theme with acrylic effects",
            "- Smooth animations and hover states",
            "- Keyboard shortcuts (Ctrl+T, Ctrl+W, Ctrl+L, Ctrl+Shift+N)"
        };
        
        for (int i = 0; i < 8; i++) {
            draw_string_scaled(text_x + 20, text_y, features[i], COL_TEXT_SECONDARY, 1);
            text_y += 20;
        }
        
        browser.max_scroll_y = text_y - browser.content_y;
    }
    
    /* Scrollbar */
    if (browser.max_scroll_y > browser.content_h) {
        int sb_x = browser.content_x + browser.content_w - 12;
        int sb_y = browser.content_y;
        int sb_w = 8;
        int sb_h = browser.content_h;
        
        fill_rect(sb_x, sb_y, sb_w, sb_h, COL_SCROLLBAR_BG);
        
        float scroll_ratio = (float)browser.content_h / browser.max_scroll_y;
        int thumb_h = (int)(sb_h * scroll_ratio);
        if (thumb_h < 40) thumb_h = 40;
        int thumb_y = sb_y + (int)((float)browser.scroll_y / browser.max_scroll_y * (sb_h - thumb_h));
        
        fill_round_rect(sb_x + 2, thumb_y, sb_w - 4, thumb_h, 2, COL_SCROLLBAR_THUMB);
    }
}

static void browser_draw_downloads(void) {
    if (!browser.show_downloads || browser.download_count == 0) return;
    
    int panel_w = 320;
    int panel_x = browser.width - panel_w;
    int panel_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT + (browser.show_bookmarks ? BROWSER_BOOKMARKS_HEIGHT : 0);
    int item_h = 40;
    int panel_h = browser.download_count * item_h + 40;
    
    if (panel_y + panel_h > browser.height) panel_h = browser.height - panel_y;
    
    /* Panel background */
    draw_shadow(panel_x - 4, panel_y - 4, panel_w + 8, panel_h + 8, 8, 4);
    fill_round_rect(panel_x, panel_y, panel_w, panel_h, 8, 0x1F1F1F);
    draw_round_rect(panel_x, panel_y, panel_w, panel_h, 8, COL_SEPARATOR);
    
    /* Header */
    draw_string_scaled(panel_x + 16, panel_y + 12, "Downloads", COL_TEXT_PRIMARY, 1);
    draw_string_scaled(panel_x + panel_w - 50, panel_y + 12, "Clear all", COL_LINK, 1);
    
    for (int i = 0; i < browser.download_count; i++) {
        int item_y = panel_y + 40 + i * item_h;
        if (item_y + item_h > panel_y + panel_h) break;
        
        fill_round_rect(panel_x + 8, item_y, panel_w - 16, item_h - 4, 4, 0x252525);
        
        /* File icon */
        fill_round_rect(panel_x + 16, item_y + 4, 32, 32, 4, COL_ACCENT_BLUE);
        draw_string_scaled(panel_x + 22, item_y + 10, "PDF", 0xFFFFFF, 1);
        
        /* File name */
        draw_string_scaled(panel_x + 56, item_y + 4, browser.download_items[i], COL_TEXT_PRIMARY, 1);
        
        /* Progress bar */
        int prog_w = (int)((panel_w - 80) * browser.download_progress[i]);
        fill_round_rect(panel_x + 56, item_y + 22, prog_w, 4, 2, COL_ACCENT_BLUE);
        
        /* Percentage */
        char pct[16];
        browser_sprintf(pct, "%d%%", (int)(browser.download_progress[i] * 100));
        draw_string_scaled(panel_x + panel_w - 60, item_y + 4, pct, COL_TEXT_MUTED, 1);
    }
}

static void browser_draw(void) {
    if (!browser.window) return;
    
    browser_draw_tab_bar();
    browser_draw_nav_buttons();
    browser_draw_url_bar();
    browser_draw_bookmarks_bar();
    browser_draw_page_content();
    browser_draw_downloads();
}

/* Input handling */
static void browser_handle_mouse_move(int x, int y) {
    mouse_state_t mouse = mouse_get_state();
    
    /* Adjust for window position */
    int wx = x - browser.window->x;
    int wy = y - browser.window->y;
    
    /* Tab bar hover */
    browser.hovered_tab = -1;
    if (wy >= 0 && wy < BROWSER_TAB_HEIGHT) {
        int tx = browser.tabs_start_x;
        for (int i = 0; i < browser.tab_count; i++) {
            browser_tab_t *tab = &browser.tabs[i];
            if (wx >= tab->x && wx < tab->x + tab->w) {
                browser.hovered_tab = i;
                break;
            }
            tx += tab->w + 2;
        }
        /* New tab button */
        if (wx >= browser.new_tab_btn_x && wx < browser.new_tab_btn_x + BROWSER_NEW_TAB_BTN_W &&
            wy >= (BROWSER_TAB_HEIGHT - 24) / 2 && wy < (BROWSER_TAB_HEIGHT + 24) / 2) {
            browser.hovered_tab = -2;
        }
    }
    
    /* Nav buttons hover */
    for (int i = 0; i < NAV_BTN_COUNT; i++) {
        nav_button_t *btn = &browser.nav_buttons[i];
        btn->hovered = (wx >= btn->x && wx < btn->x + btn->w && 
                        wy >= btn->y && wy < btn->y + btn->h && btn->enabled);
    }
    
    /* URL bar hover */
    bool url_hover = (wx >= browser.url_bar_x && wx < browser.url_bar_x + browser.url_bar_w &&
                      wy >= browser.url_bar_y && wy < browser.url_bar_y + browser.url_bar_h);
    (void)url_hover;
    
    /* Bookmarks hover */
    browser.hovered_bookmark = -1;
    if (browser.show_bookmarks) {
        int bar_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT;
        int bx = 12;
        for (int i = 0; i < browser.bookmark_count; i++) {
            int name_w = str_width_scaled(browser.bookmarks[i].name, 1);
            int item_w = name_w + 20 + 24;
            if (wy >= bar_y && wy < bar_y + BROWSER_BOOKMARKS_HEIGHT &&
                wx >= bx && wx < bx + item_w) {
                browser.hovered_bookmark = i;
                break;
            }
            bx += item_w + 4;
        }
    }
    
    /* Tab dragging */
    if (browser.drag_tab >= 0 && (mouse.buttons & 0x01)) {
        int delta = wx - browser.drag_start_x;
        browser.tabs[browser.drag_tab].x = browser.drag_start_tab_x + delta;
    }
}

static void browser_handle_mouse_down(int x, int y, int button) {
    if (button != 1) return;
    
    int wx = x - browser.window->x;
    int wy = y - browser.window->y;
    (void)wx; (void)wy;
    
    /* Tab clicks */
    if (wy >= 0 && wy < BROWSER_TAB_HEIGHT) {
        int tx = browser.tabs_start_x;
        for (int i = 0; i < browser.tab_count; i++) {
            browser_tab_t *tab = &browser.tabs[i];
            if (wx >= tab->x && wx < tab->x + tab->w) {
                /* Check close button */
                if (i == browser.active_tab && !tab->pinned) {
                    int close_x = tab->x + tab->w - 22;
                    int close_y = (BROWSER_TAB_HEIGHT - 16) / 2;
                    if (wx >= close_x && wx < close_x + 16 && wy >= close_y && wy < close_y + 16) {
                        browser_close_tab(i);
                        return;
                    }
                }
                
                /* Start drag */
                browser.drag_tab = i;
                browser.drag_start_x = wx;
                browser.drag_start_tab_x = tab->x;
                
                /* Switch tab on click */
                if (i != browser.active_tab) {
                    browser_switch_tab(i);
                }
                return;
            }
            tx += tab->w + 2;
        }
        
        /* New tab button */
        if (wx >= browser.new_tab_btn_x && wx < browser.new_tab_btn_x + BROWSER_NEW_TAB_BTN_W &&
            wy >= (BROWSER_TAB_HEIGHT - 24) / 2 && wy < (BROWSER_TAB_HEIGHT + 24) / 2) {
            browser_add_tab(false);
            return;
        }
    }
    
    /* Nav buttons */
    for (int i = 0; i < NAV_BTN_COUNT; i++) {
        nav_button_t *btn = &browser.nav_buttons[i];
        if (btn->enabled && wx >= btn->x && wx < btn->x + btn->w &&
            wy >= btn->y && wy < btn->y + btn->h) {
            btn->pressed = true;
            switch (i) {
                case NAV_BTN_BACK: browser_go_back(); break;
                case NAV_BTN_FORWARD: browser_go_forward(); break;
                case NAV_BTN_RELOAD: browser_reload(); break;
                case NAV_BTN_HOME: browser_go_home(); break;
            }
            return;
        }
    }
    
    /* URL bar */
    if (wx >= browser.url_bar_x && wx < browser.url_bar_x + browser.url_bar_w &&
        wy >= browser.url_bar_y && wy < browser.url_bar_y + browser.url_bar_h) {
        browser.url_focused = true;
        browser.url_select_all = true;
        return;
    } else {
        browser.url_focused = false;
        browser.url_select_all = false;
    }
    
    /* Go button in URL bar */
    int go_x = browser.url_bar_x + browser.url_bar_w - 36;
    int go_y = browser.url_bar_y + (browser.url_bar_h - 24) / 2;
    if (wx >= go_x && wx < go_x + 28 && wy >= go_y && wy < go_y + 24) {
        if (browser.tabs[browser.active_tab].loading) {
            browser.tabs[browser.active_tab].loading = false;
            browser.tabs[browser.active_tab].load_progress = 0.0f;
            browser.tabs[browser.active_tab].state = TAB_STATE_ACTIVE;
        } else {
            browser_handle_url_enter();
        }
        return;
    }
    
    /* Bookmarks */
    if (browser.show_bookmarks) {
        int bar_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT;
        int bx = 12;
        for (int i = 0; i < browser.bookmark_count; i++) {
            int name_w = str_width_scaled(browser.bookmarks[i].name, 1);
            int item_w = name_w + 20 + 24;
            if (wy >= bar_y && wy < bar_y + BROWSER_BOOKMARKS_HEIGHT &&
                wx >= bx && wx < bx + item_w) {
                browser_navigate(browser.bookmarks[i].url);
                return;
            }
            bx += item_w + 4;
        }
        
        /* Add bookmark */
        int add_x = bx;
        if (wx >= add_x && wx < add_x + 24 && wy >= bar_y + 4 && wy < bar_y + BROWSER_BOOKMARKS_HEIGHT - 4) {
            /* Add current page to bookmarks */
            if (browser.bookmark_count < 16 && browser.active_tab >= 0) {
                browser_tab_t *tab = &browser.tabs[browser.active_tab];
                strncpy(browser.bookmarks[browser.bookmark_count].name, tab->title, 63);
                strncpy(browser.bookmarks[browser.bookmark_count].url, tab->url, 255);
                browser.bookmarks[browser.bookmark_count].color = tab->favicon_color;
                browser.bookmark_count++;
            }
            return;
        }
    }
    
    /* Page content - handle scrolling */
    int content_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT + (browser.show_bookmarks ? BROWSER_BOOKMARKS_HEIGHT : 0);
    if (wy >= content_y) {
        /* Click on page content - could handle links here */
    }
}

static void browser_handle_mouse_up(int x, int y, int button) {
    (void)x; (void)y;
    if (button != 1) return;
    
    /* Release nav buttons */
    for (int i = 0; i < NAV_BTN_COUNT; i++) {
        browser.nav_buttons[i].pressed = false;
    }
    
    /* End tab drag */
    if (browser.drag_tab >= 0) {
        /* Snap tab back or reorder */
        browser.drag_tab = -1;
    }
}

static void browser_handle_mouse_wheel(int delta) {
    int content_y = BROWSER_TAB_HEIGHT + BROWSER_TOOLBAR_HEIGHT + (browser.show_bookmarks ? BROWSER_BOOKMARKS_HEIGHT : 0);
    mouse_state_t mouse = mouse_get_state();
    int wy = mouse.y - browser.window->y;
    (void)wy;
    
    if (wy >= content_y && browser.max_scroll_y > browser.content_h) {
        browser.scroll_y -= delta * 40;
        if (browser.scroll_y < 0) browser.scroll_y = 0;
        if (browser.scroll_y > browser.max_scroll_y - browser.content_h) {
            browser.scroll_y = browser.max_scroll_y - browser.content_h;
        }
    }
}

static void browser_handle_key_down(int key) {
    /* Handle URL bar input */
    if (browser.url_focused) {
        switch (key) {
            case KEY_ENTER:
                browser_handle_url_enter();
                return;
            case KEY_ESCAPE:
                browser.url_focused = false;
                browser.url_select_all = false;
                return;
            case KEY_BACKSPACE:
                if (browser.url_select_all) {
                    browser.url_buffer[0] = '\0';
                    browser.url_cursor_pos = 0;
                    browser.url_select_all = false;
                } else if (browser.url_cursor_pos > 0) {
                    int len = strlen(browser.url_buffer);
                    memmove(&browser.url_buffer[browser.url_cursor_pos - 1],
                            &browser.url_buffer[browser.url_cursor_pos],
                            len - browser.url_cursor_pos + 1);
                    browser.url_cursor_pos--;
                }
                return;
            case KEY_LEFT:
                if (!browser.url_select_all && browser.url_cursor_pos > 0) browser.url_cursor_pos--;
                browser.url_select_all = false;
                return;
            case KEY_RIGHT:
                if (!browser.url_select_all && browser.url_cursor_pos < (int)strlen(browser.url_buffer)) browser.url_cursor_pos++;
                browser.url_select_all = false;
                return;
            case KEY_HOME:
                if (!browser.url_select_all) browser.url_cursor_pos = 0;
                browser.url_select_all = false;
                return;
            case KEY_END:
                if (!browser.url_select_all) browser.url_cursor_pos = strlen(browser.url_buffer);
                browser.url_select_all = false;
                return;
            default:
                if (key >= 0x20 && key <= 0x7E && browser.url_cursor_pos < 511) {
                    if (browser.url_select_all) {
                        browser.url_buffer[0] = '\0';
                        browser.url_cursor_pos = 0;
                        browser.url_select_all = false;
                    }
                    int len = strlen(browser.url_buffer);
                    memmove(&browser.url_buffer[browser.url_cursor_pos + 1],
                            &browser.url_buffer[browser.url_cursor_pos],
                            len - browser.url_cursor_pos + 1);
                    browser.url_buffer[browser.url_cursor_pos] = (char)key;
                    browser.url_cursor_pos++;
                }
                return;
        }
    }
    
    /* Global shortcuts */
    uint8_t mods = keyboard_get_modifiers();
    bool ctrl = mods & KMOD_CTRL;
    bool shift = mods & KMOD_SHIFT;
    
    if (ctrl && shift && key == KEY_N) {
        /* Ctrl+Shift+N - New incognito window */
        browser_add_tab(true);
        return;
    }
    
    if (ctrl && key == KEY_T) {
        /* Ctrl+T - New tab */
        browser_add_tab(false);
        return;
    }
    
    if (ctrl && key == KEY_W) {
        /* Ctrl+W - Close tab */
        browser_close_tab(browser.active_tab);
        return;
    }
    
    if (ctrl && shift && key == KEY_T) {
        /* Ctrl+Shift+T - Reopen closed tab (not implemented) */
        return;
    }
    
    if (ctrl && key == KEY_L) {
        /* Ctrl+L - Focus address bar */
        browser.url_focused = true;
        browser.url_select_all = true;
        return;
    }
    
    if (ctrl && key == KEY_R) {
        /* Ctrl+R - Reload */
        browser_reload();
        return;
    }
    
    if (ctrl && key == KEY_TAB) {
        /* Ctrl+Tab - Next tab */
        browser.active_tab = (browser.active_tab + 1) % browser.tab_count;
        browser_switch_tab(browser.active_tab);
        return;
    }
    
    if (ctrl && shift && key == KEY_TAB) {
        /* Ctrl+Shift+Tab - Previous tab */
        browser.active_tab = (browser.active_tab - 1 + browser.tab_count) % browser.tab_count;
        browser_switch_tab(browser.active_tab);
        return;
    }
    
    if (ctrl && key >= KEY_1 && key <= KEY_9) {
        /* Ctrl+1-9 - Switch to tab */
        int tab_idx = key - KEY_1;
        if (tab_idx < browser.tab_count) {
            browser_switch_tab(tab_idx);
        }
        return;
    }
    
    if (key == KEY_F11) {
        /* F11 - Toggle fullscreen (not implemented) */
        return;
    }
    
    if (ctrl && key == KEY_B) {
        /* Ctrl+B - Toggle bookmarks bar */
        browser.show_bookmarks = !browser.show_bookmarks;
        return;
    }
    
    if (ctrl && key == KEY_J) {
        /* Ctrl+J - Toggle downloads */
        browser.show_downloads = !browser.show_downloads;
        return;
    }
    
    if (ctrl && shift && key == KEY_I) {
        /* Ctrl+Shift+I - Toggle incognito mode for current tab */
        if (browser.active_tab >= 0) {
            browser.tabs[browser.active_tab].incognito = !browser.tabs[browser.active_tab].incognito;
            browser.tabs[browser.active_tab].favicon_color = 
                browser.tabs[browser.active_tab].incognito ? COL_INCOGNITO_PURPLE : COL_ACCENT_BLUE;
            strcpy(browser.tabs[browser.active_tab].title, 
                   browser.tabs[browser.active_tab].incognito ? "Incognito" : "New Tab");
        }
        return;
    }
}

static void browser_handle_key_up(int key) {
    (void)key;
}

static void browser_update(void) {
    /* Update animations */
    if (browser.animating_tab >= 0 && browser.tab_anim_progress < 1.0f) {
        browser.tab_anim_progress += 0.1f;
        if (browser.tab_anim_progress >= 1.0f) {
            browser.tab_anim_progress = 1.0f;
            browser.animating_tab = -1;
        }
    }
    
    /* Update download progress */
    for (int i = 0; i < browser.download_count; i++) {
        if (browser.download_progress[i] < 1.0f) {
            browser.download_progress[i] += 0.005f;
            if (browser.download_progress[i] >= 1.0f) {
                browser.download_progress[i] = 1.0f;
            }
        }
    }
    
    browser.last_frame_time = timer_get_ticks();
}

int main(void) {
    browser_init();
    
    if (!browser.window) {
        kprintf("[Browser] Failed to create browser window\n");
        return 1;
    }
    
    /* Main event loop */
    while (true) {
        browser_update();
        
        /* Process keyboard events */
        key_event_t key_event;
        while (keyboard_get_event(&key_event)) {
            if (key_event.type == KEY_EVENT_DOWN) {
                browser_handle_key_down(key_event.keycode);
            } else if (key_event.type == KEY_EVENT_UP) {
                browser_handle_key_up(key_event.keycode);
            }
        }
        
        /* Process mouse events */
        mouse_state_t mouse = mouse_get_state();
        if (mouse.present) {
            fb_info_t *fb = fb_get_info();
            if (fb && fb->width > 0 && fb->height > 0) {
                int mx = mouse.x;
                int my = mouse.y;
                
                if (mx < 0) mx = 0;
                if (mx >= (int)fb->width) mx = fb->width - 1;
                if (my < 0) my = 0;
                if (my >= (int)fb->height) my = fb->height - 1;
                
                /* Check if mouse is over our window */
                if (mx >= browser.window->x && mx < browser.window->x + browser.window->width &&
                    my >= browser.window->y && my < browser.window->y + browser.window->height) {
                    
                    static int last_mx = -1, last_my = -1;
                    static uint8_t last_buttons = 0;
                    
                    if (mx != last_mx || my != last_my) {
                        browser_handle_mouse_move(mx, my);
                        last_mx = mx;
                        last_my = my;
                    }
                    
                    uint8_t btn_changed = mouse.buttons ^ last_buttons;
                    if (btn_changed) {
                        if (btn_changed & 0x01) {
                            if (mouse.buttons & 0x01) {
                                browser_handle_mouse_down(mx, my, 1);
                            } else {
                                browser_handle_mouse_up(mx, my, 1);
                            }
                        }
                        if (btn_changed & 0x02) {
                            if (mouse.buttons & 0x02) {
                                browser_handle_mouse_down(mx, my, 2);
                            } else {
                                browser_handle_mouse_up(mx, my, 2);
                            }
                        }
                        if (btn_changed & 0x04) {
                            if (mouse.buttons & 0x04) {
                                browser_handle_mouse_down(mx, my, 3);
                            } else {
                                browser_handle_mouse_up(mx, my, 3);
                            }
                        }
                        last_buttons = mouse.buttons;
                    }
                    
                    if (mouse.scroll != 0) {
                        browser_handle_mouse_wheel(mouse.scroll);
                    }
                }
            }
        }
        
        /* Redraw */
        browser_draw();
        wm_draw_cursor();
        
        /* Frame rate limiting */
        uint32_t frame_time = timer_get_ticks() - browser.last_frame_time;
        if (frame_time < 16) {
            /* Busy wait instead of sleep_ms (no syscalls in kernel mode) */
            uint32_t start = timer_get_ticks();
            while (timer_get_ticks() - start < (16 - frame_time)) {
                __asm__("pause");
            }
        }
        
        /* yield() - no-op in kernel mode */
        __asm__("pause");
    }
    
    return 0;
}