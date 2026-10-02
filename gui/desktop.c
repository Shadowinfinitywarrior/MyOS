#include "desktop.h"
#include "theme.h"
#include "text.h"
#include "util.h"
#include "input.h"
#include "cursor.h"
#include "term.h"
#include "apps.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../kernel/vtty.h"
#include "../kernel/process.h"
#include "../drivers/framebuffer.h"
#include "../drivers/rtc.h"

/* ---- layout ------------------------------------------------------------- */

static int scr_w, scr_h;
static int taskbar_h = 44;
static int workarea_bottom;

#define START_BTN_W 92
#define TASKBTN_H   32
#define TASKBTN_MAX 8

typedef struct icon_slot {
    const app_entry_t *app;
    int  x, y;
    bool hot;
} icon_slot_t;

static icon_slot_t icons[DESKTOP_MAX_ICONS];
static int         icon_count;
static bool        menu_open;
static int         menu_hot = -1;
static int         start_hot;
static int         task_hot = -1;
static bool        wallpaper_done;
static uint32_t    last_clock_s;
static char        clock_buf[32];

int desktop_workarea_bottom(void) { return workarea_bottom; }

/* ---- app registry -------------------------------------------------------- */

static void launch_terminal(void)  { term_open_shell(); }
static void launch_files(void)     { app_open_files(); }
static void launch_about(void)     { app_open_about(); }
static void launch_help(void)      { app_open_help(); }
static void launch_sysinfo(void)   { app_open_sysinfo(); }

/* 16x16 1bpp icons, two bytes per row. Kept inline so the registry has no
 * external asset to lose. */
static const uint8_t icon_terminal[32] = {
    0x00,0x00,
    0xfc,0x3f,
    0x04,0x20,
    0x04,0x20,
    0x04,0x20,
    0x04,0x10,
    0x04,0x10,
    0x04,0x10,
    0x04,0x10,
    0xfc,0x3f,
    0x00,0x00,
    0x10,0x08,
    0x20,0x04,
    0x40,0x02,
    0x80,0x01,
    0x00,0x00,
};
static const uint8_t icon_files[32] = {
    0x00,0x00,
    0x00,0x00,
    0x78,0x00,
    0x48,0x00,
    0x48,0x08,
    0xc8,0x07,
    0x08,0x04,
    0x08,0x04,
    0x08,0x04,
    0x08,0x04,
    0xff,0x0f,
    0x08,0x04,
    0x08,0x04,
    0x08,0x04,
    0x08,0x04,
    0xf8,0x07,
};
static const uint8_t icon_about[32] = {
    0x00,0x00,
    0xfc,0x3f,
    0x04,0x20,
    0x04,0x20,
    0x04,0x20,
    0x04,0x20,
    0x04,0x20,
    0x84,0x21,
    0xc4,0x23,
    0x84,0x21,
    0x04,0x20,
    0x04,0x20,
    0x04,0x20,
    0xfc,0x3f,
    0x00,0x00,
    0x00,0x00,
};
static const uint8_t icon_help[32] = {
    0x00,0x00,
    0x00,0x00,
    0xc0,0x00,
    0x20,0x01,
    0x20,0x01,
    0xc0,0x00,
    0x80,0x00,
    0x80,0x00,
    0x00,0x00,
    0x80,0x00,
    0x80,0x00,
    0x00,0x00,
    0x00,0x00,
    0x00,0x00,
    0x00,0x00,
    0x00,0x00,
};
static const uint8_t icon_monitor[32] = {
    0x00,0x00,
    0xfc,0x3f,
    0x04,0x20,
    0xf4,0x2f,
    0x14,0x28,
    0x14,0x28,
    0x14,0x28,
    0x14,0x28,
    0x14,0x28,
    0xf4,0x2f,
    0x04,0x20,
    0xfc,0x3f,
    0xc0,0x03,
    0x00,0x00,
    0x00,0x00,
    0x00,0x00,
};

static app_entry_t apps[] = {
    { "Terminal",  "Shell session",     launch_terminal, icon_terminal },
    { "Files",     "Browse the disks",  launch_files,    icon_files },
    { "System",    "Processes + memory", launch_sysinfo,  icon_monitor },
    { "Help",      "Keys and mouse",    launch_help,     icon_help },
    { "About",     "System information", launch_about,    icon_about },
};

#define APP_COUNT ((int)ARRAY_SIZE(apps))

int desktop_app_count(void) { return APP_COUNT; }

const app_entry_t *desktop_app_at(int i) {
    if (i < 0 || i >= APP_COUNT) return NULL;
    return &apps[i];
}

bool desktop_launch(const char *name) {
    for (int i = 0; i < APP_COUNT; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            apps[i].launch();
            desktop_invalidate();
            return true;
        }
    }
    return false;
}

/* ---- wallpaper ----------------------------------------------------------- */

/* The wallpaper is rendered once into a surface and blitted each frame. The
 * gradient plus glow is a per-pixel computation; recomputing it every frame
 * would dominate the frame budget, while a straight blit is a memory copy. */
static surface_t *wallpaper_surf;

static void draw_wallpaper(void) {
    if (!wallpaper_surf) {
        wallpaper_surf = surface_create(scr_w, scr_h);
        if (!wallpaper_surf) return;
    }
    surface_t *s = wallpaper_surf;
    uint32_t *p = s->pixels;
    int stride = s->pitch / 4;
    int half = scr_h / 2;

    /* A three-stop vertical gradient, computed once. */
    for (int y = 0; y < scr_h; y++) {
        color_t from = (y < half) ? TH_BG_DEEP : TH_BG_MID;
        color_t to   = (y < half) ? TH_BG_MID   : TH_BG_LOW;
        uint32_t t = (uint32_t)((uint64_t)((y < half) ? y : (y - half)) * 255 /
                                 (uint64_t)((y < half) ? half : (scr_h - half)));
        uint32_t fr = (from >> 16) & 0xFF, fg = (from >> 8) & 0xFF, fb_ = from & 0xFF;
        uint32_t tr = (to >> 16) & 0xFF,   tg = (to >> 8) & 0xFF,   tb = to & 0xFF;
        uint32_t c = (((fr + ((tr - fr) * t) / 255) << 16) |
                      ((fg + ((tg - fg) * t) / 255) << 8) |
                       (fb_ + ((tb - fb_) * t) / 255));
        uint32_t *row = p + (size_t)y * (size_t)stride;
        for (int x = 0; x < scr_w; x++) row[x] = c;
    }

    /* A soft accent glow behind the desktop, as a radial falloff. */
    int cx = scr_w / 2, cy = scr_h / 3;
    int rad = scr_w < scr_h ? scr_w : scr_h;
    uint32_t rad2 = (uint32_t)rad * (uint32_t)rad;
    for (int y = 0; y < scr_h; y++) {
        int dy = y - cy;
        uint32_t dy2 = (uint32_t)(dy * dy);
        uint32_t *row = p + (size_t)y * (size_t)stride;
        for (int x = 0; x < scr_w; x++) {
            int dx = x - cx;
            uint32_t d2 = dy2 + (uint32_t)(dx * dx);
            if (d2 > rad2) continue;
            uint32_t t = (255 - d2 * 255 / rad2) / 6;
            uint32_t c = row[x];
            uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            r += (((TH_ACCENT_DEEP >> 16) & 0xFF) - r) * t / 255;
            g += (((TH_ACCENT_DEEP >> 8) & 0xFF) - g) * t / 255;
            b += ((TH_ACCENT_DEEP & 0xFF) - b) * t / 255;
            row[x] = (r << 16) | (g << 8) | b;
        }
    }
    wallpaper_done = true;
}

/* Copy the wallpaper over the work area, so windows leave no trails when they
 * move or close. */
static void blit_wallpaper(uint32_t *bb, int stride) {
    if (!wallpaper_surf) return;
    blit_copy_offset(bb, stride * 4, 0, 0,
                     wallpaper_surf->pixels, wallpaper_surf->pitch, 0, 0,
                     scr_w, workarea_bottom);
}

/* ---- drawing helpers on the back buffer ---------------------------------- */

static inline void px(uint32_t *bb, int stride, int x, int y, color_t c) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) return;
    bb[(size_t)y * (size_t)stride + x] = c;
}

static void draw_rect_aa(uint32_t *bb, int stride, const rect_t *r, color_t c,
                         int alpha /* 0..255 */) {
    if (r->w <= 0 || r->h <= 0) return;
    if (alpha >= 255) {
        for (int y = r->y; y < r->y + r->h; y++)
            for (int x = r->x; x < r->x + r->w; x++) px(bb, stride, x, y, c);
        return;
    }
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = r->x; x < r->x + r->w; x++) {
            if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) continue;
            uint32_t d = bb[(size_t)y * (size_t)stride + x];
            uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
            uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
            uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
            bb[(size_t)y * (size_t)stride + x] = (r0 << 16) | (g0 << 8) | b0;
        }
    }
}

static void draw_round_rect(uint32_t *bb, int stride, const rect_t *r, int radius,
                            color_t c, int alpha) {
    if (r->w <= 0 || r->h <= 0) return;
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;
    rect_t body = { r->x, r->y + radius, r->w, r->h - radius * 2 };
    draw_rect_aa(bb, stride, &body, c, alpha);
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, radius };
    draw_rect_aa(bb, stride, &top, c, alpha);
    rect_t bot = { r->x + radius, r->y + r->h - radius, r->w - radius * 2, radius };
    draw_rect_aa(bb, stride, &bot, c, alpha);
    int rr = radius * radius;
    int corners[4][2] = {
        { r->x + radius, r->y + radius },
        { r->x + r->w - radius - 1, r->y + radius },
        { r->x + radius, r->y + r->h - radius - 1 },
        { r->x + r->w - radius - 1, r->y + r->h - radius - 1 },
    };
    for (int k = 0; k < 4; k++) {
        int cx = corners[k][0], cy = corners[k][1];
        for (int dy = 0; dy < radius; dy++)
            for (int dx = 0; dx < radius; dx++) {
                int ex = (k & 1) ? (radius - 1 - dx) : dx;
                int ey = (k & 2) ? (radius - 1 - dy) : dy;
                if (ex * ex + ey * ey > rr) continue;
                int x = (k & 1) ? (cx + ex) : (cx + ex);
                int y = (k & 2) ? (cy + ey) : (cy + ey);
                if (alpha >= 255) px(bb, stride, x, y, c);
                else {
                    int ax = x, ay = y;
                    if (ax < 0 || ay < 0 || ax >= scr_w || ay >= scr_h) continue;
                    uint32_t d = bb[(size_t)ay * (size_t)stride + ax];
                    uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
                    uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
                    uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
                    bb[(size_t)ay * (size_t)stride + ax] = (r0 << 16) | (g0 << 8) | b0;
                }
            }
    }
}

static void draw_round_outline(uint32_t *bb, int stride, const rect_t *r, int radius,
                               color_t c, int thickness, int alpha) {
    if (r->w <= 0 || r->h <= 0 || thickness <= 0) return;
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, thickness };
    draw_rect_aa(bb, stride, &top, c, alpha);
    rect_t bot = { r->x + radius, r->y + r->h - thickness, r->w - radius * 2, thickness };
    draw_rect_aa(bb, stride, &bot, c, alpha);
    rect_t lft = { r->x, r->y + radius, thickness, r->h - radius * 2 };
    draw_rect_aa(bb, stride, &lft, c, alpha);
    rect_t rgt = { r->x + r->w - thickness, r->y + radius, thickness, r->h - radius * 2 };
    draw_rect_aa(bb, stride, &rgt, c, alpha);
    int ro = radius * radius;
    int inner = radius - thickness;
    int ri = inner > 0 ? inner * inner : 0;
    for (int q = 0; q < 4; q++) {
        int sx = (q & 1) ? -1 : 1, sy = (q & 2) ? -1 : 1;
        int cx = (q & 1) ? (r->x + r->w - radius - 1) : (r->x + radius);
        int cy = (q & 2) ? (r->y + r->h - radius - 1) : (r->y + radius);
        for (int dy = 0; dy < radius; dy++)
            for (int dx = 0; dx < radius; dx++) {
                int ox = sx * dx, oy = sy * dy;
                int d2 = ox * ox + oy * oy;
                if (d2 > ro || d2 < ri) continue;
                for (int t = 0; t < thickness; t++)
                    px(bb, stride, cx + ox - sx * t, cy + oy - sy * t, c);
            }
    }
}

static void draw_icon(uint32_t *bb, int stride, int x, int y, const uint8_t *icon,
                      color_t color) {
    for (int row = 0; row < 16; row++) {
        uint16_t bits = (uint16_t)(icon[row * 2] | (icon[row * 2 + 1] << 8));
        for (int col = 0; col < 16; col++)
            if (bits & (1u << col)) px(bb, stride, x + col, y + row, color);
    }
}

/* Draw text directly to the back buffer by routing through a scratch surface
 * is wasteful; instead blit glyphs straight into the framebuffer. */
static void draw_text_bb(uint32_t *bb, int stride, const baked_font_t *f, int x, int y,
                         const char *str, color_t c) {
    int pen = x;
    for (int i = 0; str[i]; ) {
        uint32_t cp;
        i += utf8_decode(str + i, &cp);
        if (cp < (uint32_t)f->first_cp || cp > 126) continue;
        int gi = (int)cp - f->first_cp;
        int gw = f->gw[gi], gh = f->gh[gi];
        if (gw > 0 && gh > 0) {
            int stride_b = (gw + 7) / 8;
            const uint8_t *bits = f->bitmap + f->bo[gi];
            for (int r = 0; r < gh; r++)
                for (int col = 0; col < gw; col++)
                    if (bits[r * stride_b + (col >> 3)] & (1 << (col & 7)))
                        px(bb, stride, pen + f->xo[gi] + col, y + f->yo[gi] + r, c);
        }
        pen += f->xa[gi];
    }
}

/* ---- clock --------------------------------------------------------------- */

void desktop_format_clock(char *buf, int len, bool with_seconds) {
    datetime_t t;
    rtc_get_time(&t);
    ui_strcpy(buf, len, "");
    ui_cat_num(buf, len, t.hour, ":");
    ui_cat_num(buf, len, t.minute, "");
    if (with_seconds) { ui_cat(buf, len, ":"); ui_cat_num(buf, len, t.second, ""); }
}

/* ---- taskbar -------------------------------------------------------------- */

static int taskbtn_x(int i) {
    int x = START_BTN_W + 12;
    for (int k = 0; k < i; k++) x += TASKBTN_H + 34;
    return x;
}

static void draw_taskbar(uint32_t *bb, int stride) {
    rect_t bar = { 0, scr_h - taskbar_h, scr_w, taskbar_h };
    draw_rect_aa(bb, stride, &bar, TH_TASKBAR, 255);
    rect_t edge = { 0, scr_h - taskbar_h, scr_w, 1 };
    draw_rect_aa(bb, stride, &edge, TH_TASKBAR_EDGE, 255);

    /* Start button. */
    rect_t sb = { 8, scr_h - taskbar_h + 6, START_BTN_W - 8, taskbar_h - 12 };
    color_t sb_bg = menu_open ? TH_ACCENT : (start_hot ? TH_BTN_HOVER : TH_ACCENT_SOFT);
    draw_round_rect(bb, stride, &sb, 8, sb_bg, 255);
    draw_icon(bb, stride, sb.x + 9, sb.y + (sb.h - 16) / 2,
              apps[0].icon, menu_open ? TH_TEXT_BRIGHT : TH_TEXT);
    draw_text_bb(bb, stride, font_bold(), sb.x + 32, sb.y + (sb.h - 15) / 2,
                 "Start", menu_open ? TH_TEXT_BRIGHT : TH_TEXT);

    /* One task button per window. */
    int n = wm_window_count();
    for (int i = 0; i < n && i < TASKBTN_MAX; i++) {
        wm_window_t *w = wm_window_by_creation(i);
        if (!w) continue;
        int bx = taskbtn_x(i);
        int by = scr_h - taskbar_h + (taskbar_h - TASKBTN_H) / 2;
        rect_t b = { bx, by, 130, TASKBTN_H };
        bool hot = (task_hot == i);
        color_t bg = w->focused ? TH_ACCENT_SOFT : (hot ? TH_BTN_HOVER : TH_TASKBAR);
        draw_round_rect(bb, stride, &b, 7, bg, 255);
        if (w->focused) {
            rect_t ind = { bx + 3, by + 6, 3, TASKBTN_H - 12 };
            draw_round_rect(bb, stride, &ind, 2, TH_ACCENT, 255);
        }
        /* Truncate the title to fit the button. */
        char title[32];
        int tn = 0;
        while (w->title[tn] && tn < 24) { title[tn] = w->title[tn]; tn++; }
        title[tn] = 0;
        if (w->flags & WF_MINIMIZED) {
            title[tn] = ' '; title[tn + 1] = 0;
        }
        draw_text_bb(bb, stride, font_ui(), bx + 12, by + (TASKBTN_H - 15) / 2, title,
                     w->focused ? TH_TEXT : TH_TEXT_DIM);
    }

    /* Tray: uptime and clock. */
    if (last_clock_s == 0) desktop_format_clock(clock_buf, sizeof(clock_buf), true);
    char uptime[24];
    ui_strcpy(uptime, sizeof(uptime), "");
    ui_cat_num(uptime, sizeof(uptime), timer_get_seconds() / 60, "m up");
    int cw = text_width(font_mono(), clock_buf);
    draw_text_bb(bb, stride, font_mono(), scr_w - 18 - cw, scr_h - taskbar_h + 14,
                 clock_buf, TH_TEXT);
    int uw = text_width(font_ui(), uptime);
    draw_text_bb(bb, stride, font_ui(), scr_w - 18 - cw - 20 - uw,
                 scr_h - taskbar_h + 15, uptime, TH_TEXT_DIM);
}

static void draw_menu(uint32_t *bb, int stride) {
    if (!menu_open) return;
    int mw = 260, mh = APP_COUNT * 44 + 16;
    int mx = 8, my = scr_h - taskbar_h - mh - 6;
    rect_t card = { mx, my, mw, mh };

    /* Drop shadow. */
    rect_t sh = { mx + 3, my + 5, mw, mh };
    draw_round_rect(bb, stride, &sh, 12, RGB(0, 0, 0), 90);
    draw_round_rect(bb, stride, &card, 12, TH_WIN_BG, 250);
    draw_round_outline(bb, stride, &card, 12, TH_BORDER, 1, 255);

    for (int i = 0; i < APP_COUNT; i++) {
        int iy = my + 8 + i * 44;
        rect_t row = { mx + 8, iy, mw - 16, 40 };
        if (menu_hot == i) draw_round_rect(bb, stride, &row, 8, TH_ACCENT_SOFT, 255);
        rect_t ic = { mx + 16, iy + 4, 32, 32 };
        draw_round_rect(bb, stride, &ic, 7, TH_ACCENT_DEEP, 255);
        draw_icon(bb, stride, ic.x + 8, ic.y + 8, apps[i].icon, TH_TEXT_BRIGHT);
        draw_text_bb(bb, stride, font_bold(), mx + 58, iy + 4, apps[i].name, TH_TEXT);
        draw_text_bb(bb, stride, font_ui(), mx + 58, iy + 21, apps[i].desc, TH_TEXT_DIM);
    }
}

static void draw_desktop_icons(uint32_t *bb, int stride) {
    for (int i = 0; i < icon_count; i++) {
        icon_slot_t *s = &icons[i];
        rect_t plate = { s->x, s->y, 84, 78 };
        if (s->hot) draw_round_rect(bb, stride, &plate, 8, TH_ACCENT_SOFT, 140);
        draw_icon(bb, stride, s->x + 34, s->y + 8, s->app->icon, TH_TEXT);
        const char *nm = s->app->name;
        int tw = text_width(font_ui(), nm);
        draw_text_bb(bb, stride, font_ui(), s->x + (84 - tw) / 2, s->y + 32, nm, TH_TEXT);
        int dw = text_width(font_ui(), s->app->desc);
        if (dw < 84)
            draw_text_bb(bb, stride, font_ui(), s->x + (84 - dw) / 2, s->y + 50,
                         s->app->desc, TH_TEXT_DIM);
    }
}

/* ---- init ---------------------------------------------------------------- */

void desktop_init(int w, int h) {
    scr_w = w;
    scr_h = h;
    workarea_bottom = h - taskbar_h;
    menu_open = false;
    menu_hot = -1;
    start_hot = 0;
    task_hot = -1;
    wallpaper_done = false;
    icon_count = 0;
    for (int i = 0; i < APP_COUNT && i < DESKTOP_MAX_ICONS; i++) {
        icons[icon_count].app = &apps[i];
        icons[icon_count].x = 26;
        icons[icon_count].y = 22 + i * 96;
        icons[icon_count].hot = false;
        icon_count++;
    }
    term_register_service();
    desktop_format_clock(clock_buf, sizeof(clock_buf), true);
    last_clock_s = 1;
}

void desktop_shutdown(void) { wm_shutdown(); }

void desktop_invalidate(void) { wallpaper_done = false; }

void desktop_invalidate_rect(const rect_t *r) {
    if (r) fb_add_damage(r->x, r->y, r->w, r->h);
}

/* ---- hit testing for shell chrome ---------------------------------------- */

static bool in_start(int x, int y) {
    rect_t sb = { 8, scr_h - taskbar_h + 6, START_BTN_W - 8, taskbar_h - 12 };
    return rect_contains_point(&sb, x, y);
}

static bool in_taskbtn(int x, int y, int *idx) {
    if (y < scr_h - taskbar_h || y >= scr_h) return false;
    int n = wm_window_count();
    for (int i = 0; i < n && i < TASKBTN_MAX; i++) {
        rect_t b = { taskbtn_x(i), scr_h - taskbar_h + (taskbar_h - TASKBTN_H) / 2,
                     130, TASKBTN_H };
        if (rect_contains_point(&b, x, y)) { if (idx) *idx = i; return true; }
    }
    return false;
}

static int menu_index_at(int x, int y) {
    if (!menu_open) return -1;
    int mw = 260, mh = APP_COUNT * 44 + 16;
    int mx = 8, my = scr_h - taskbar_h - mh - 6;
    for (int i = 0; i < APP_COUNT; i++) {
        rect_t row = { mx + 8, my + 8 + i * 44, mw - 16, 40 };
        if (rect_contains_point(&row, x, y)) return i;
    }
    return -1;
}

static bool desktop_click(int x, int y) {
    if (menu_open) {
        int mi = menu_index_at(x, y);
        if (mi >= 0) {
            menu_open = false;
            desktop_launch(apps[mi].name);
            return true;
        }
        /* Clicking outside the menu dismisses it. */
        int mw = 260, mh = APP_COUNT * 44 + 16;
        rect_t card = { 8, scr_h - taskbar_h - mh - 6, mw, mh };
        if (!rect_contains_point(&card, x, y)) { menu_open = false; return true; }
        return true;
    }
    if (in_start(x, y)) { menu_open = !menu_open; return true; }

    int tidx;
    if (in_taskbtn(x, y, &tidx)) {
        wm_window_t *w = wm_window_by_creation(tidx);
        if (w) {
            if (w->flags & WF_MINIMIZED) {
                w->flags &= ~WF_MINIMIZED;
                wm_focus(w);
            } else if (w->focused) {
                w->flags |= WF_MINIMIZED;
                wm_invalidate(w);
            } else {
                wm_focus(w);
            }
        }
        return true;
    }

    /* Desktop icon double-click opens the app. */
    for (int i = 0; i < icon_count; i++) {
        rect_t r = { icons[i].x, icons[i].y, 84, 78 };
        if (rect_contains_point(&r, x, y)) {
            desktop_launch(icons[i].app->name);
            return true;
        }
    }
    return false;
}

static void desktop_hover(int x, int y) {
    start_hot = in_start(x, y) ? 1 : 0;
    task_hot = -1;
    int t;
    if (in_taskbtn(x, y, &t)) task_hot = t;
    menu_hot = menu_index_at(x, y);
    for (int i = 0; i < icon_count; i++) {
        rect_t r = { icons[i].x, icons[i].y, 84, 78 };
        icons[i].hot = rect_contains_point(&r, x, y);
    }
}

/* ---- frame ---------------------------------------------------------------- */

void desktop_tick(void) {
    input_pump();

    gui_event_t ev;
    while (input_poll(&ev)) {
        if (ev.type == EV_MOUSE_MOVE) {
            desktop_hover(ev.x, ev.y);
            /* Window dragging and edge resizing are driven from the window
             * manager's mouse-move path, so the event has to reach it. Forward
             * it only while a drag or resize is in flight: passing every move
             * would also let the WM focus windows the pointer merely passes
             * over, stealing focus from the window the user actually clicked. */
            if (wm_capture_active()) wm_handle_event(&ev);
            continue;
        }
        if (ev.type == EV_MOUSE_DOWN) {
            if (desktop_click(ev.x, ev.y)) continue;
        }
        wm_handle_event(&ev);
    }

    if (term_service_ptr) term_service_ptr();

    /* The clock only needs redrawing once a second. */
    uint32_t secs = timer_get_seconds();
    if (secs != last_clock_s) {
        last_clock_s = secs;
        desktop_format_clock(clock_buf, sizeof(clock_buf), true);
        fb_add_damage(scr_w - 200, scr_h - taskbar_h, 200, taskbar_h);
    }

    uint32_t *bb = fb_get_backbuffer();
    if (!bb) return;
    int stride = fb_get_stride();

    if (!wallpaper_done) draw_wallpaper();
    blit_wallpaper(bb, stride);

    wm_compose();

    draw_desktop_icons(bb, stride);
    draw_menu(bb, stride);
    draw_taskbar(bb, stride);
    cursor_draw();

    /* The shell chrome and backdrop are written straight to the back buffer,
     * so register them as damaged for this frame's flush. */
    fb_add_damage(0, 0, scr_w, workarea_bottom);
    fb_add_damage(0, scr_h - taskbar_h, scr_w, taskbar_h);
    if (menu_open) {
        int mw = 260, mh = APP_COUNT * 44 + 16;
        fb_add_damage(8, scr_h - taskbar_h - mh - 6, mw + 8, mh + 10);
    }
    fb_flush();
}

void desktop_run(void) {
    for (;;) {
        desktop_tick();
        /* Yield so ring-3 processes (the shell in a terminal) still run. */
        process_yield();
    }
}
