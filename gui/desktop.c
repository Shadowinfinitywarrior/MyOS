#include "desktop.h"
#include "theme.h"
#include "text.h"
#include "util.h"
#include "input.h"
#include "cursor.h"
#include "term.h"
#include "apps.h"
#include "login.h"
#include "browser.h"
#include "logo.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../kernel/vtty.h"
#include "../kernel/process.h"
#include "../kernel/storage.h"
#include "../drivers/framebuffer.h"
#include "../drivers/rtc.h"
#include "../drivers/keyboard.h"
#include "../drivers/speaker.h"
#include "../kernel/power_net.h"
#include "../include/rust_gui.h"

/* ---- layout & geometry -------------------------------------------------- */

static int scr_w, scr_h;
static int topbar_h = 26;
static int dock_w = 380, dock_h = 48, dock_x, dock_y;
static int lnav_w = 46, lnav_h = 260, lnav_x = 8, lnav_y = 36;
static int workarea_bottom;

typedef struct icon_slot {
    const app_entry_t *app;
    int  x, y;
    bool hot;
} icon_slot_t;

static icon_slot_t icons[DESKTOP_MAX_ICONS];
static int         icon_count;
static bool        menu_open;
static int         menu_hot = -1;
static int         dock_hot = -1;
static int         lnav_hot = -1;
static bool        wallpaper_done;
static uint32_t    last_clock_s;
static char        clock_buf[32];
static int         hit_clock_x0 = 0, hit_clock_x1 = 0;
static int         hit_batt_x0 = 0, hit_batt_x1 = 0;
static int         hit_net_x0 = 0, hit_net_x1 = 0;

int desktop_workarea_bottom(void) { return workarea_bottom; }

/* ---- app registry -------------------------------------------------------- */

static void launch_terminal(void)   { term_open_shell(); }
static void launch_browser(void)    { app_open_browser(); }
static void launch_files(void)      { app_open_files(); }
static void launch_editor(void)     { app_open_editor(); }
static void launch_calculator(void) { app_open_calculator(); }
static void launch_player(void)     { app_open_player(); }
static void launch_settings(void)   { app_open_settings(); }
static void launch_sysinfo(void)    { app_open_sysinfo(); }
static void launch_help(void)       { app_open_help(); }
static void launch_about(void)      { app_open_about(); }

/* 16x16 1bpp icons, two bytes per row */
static const uint8_t icon_terminal[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00,
    0x18, 0x00, 0x30, 0x00, 0x60, 0x00, 0x60, 0x00,
    0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x04, 0x00,
    0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t icon_browser[32] = {
    0x00, 0x00, 0xe0, 0x07, 0x18, 0x18, 0x24, 0x24,
    0x42, 0x42, 0x99, 0x99, 0x99, 0x99, 0x81, 0x81,
    0x81, 0x81, 0x99, 0x99, 0x99, 0x99, 0x42, 0x42,
    0x24, 0x24, 0x18, 0x18, 0xe0, 0x07, 0x00, 0x00,
};
static const uint8_t icon_files[32] = {
    0x00, 0x00, 0x3c, 0x00, 0x7e, 0x00, 0xc2, 0x3f,
    0x82, 0x7f, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0xfe, 0x7f, 0xfc, 0x3f, 0x00, 0x00,
};
static const uint8_t icon_editor[32] = {
    0x00, 0x00, 0x3c, 0x00, 0x7e, 0x00, 0x66, 0x3c,
    0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x66,
    0x7e, 0x66, 0x66, 0x7e, 0x66, 0x7e, 0x66, 0x66,
    0x7e, 0x3c, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t icon_calculator[32] = {
    0x00, 0x00, 0xfc, 0x3f, 0xfe, 0x7f, 0x82, 0x41,
    0xba, 0x5d, 0x82, 0x41, 0xaa, 0x55, 0x82, 0x41,
    0xaa, 0x55, 0x82, 0x41, 0xaa, 0x55, 0x82, 0x41,
    0xfe, 0x7f, 0xfc, 0x3f, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t icon_player[32] = {
    0x00, 0x00, 0x3c, 0x3c, 0x3e, 0x3e, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x76, 0x36,
    0xf6, 0x36, 0xe6, 0x76, 0xc6, 0xe6, 0x00, 0xc6,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t icon_settings[32] = {
    0x00, 0x00, 0x80, 0x01, 0xe0, 0x07, 0x38, 0x1c,
    0x3e, 0x7c, 0x4f, 0xf2, 0xc6, 0x63, 0x80, 0x01,
    0x80, 0x01, 0xc6, 0x63, 0x4f, 0xf2, 0x3e, 0x7c,
    0x38, 0x1c, 0xe0, 0x07, 0x80, 0x01, 0x00, 0x00,
};
static const uint8_t icon_monitor[32] = {
    0x00, 0x00, 0xfc, 0x3f, 0xfe, 0x7f, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0xfe, 0x7f,
    0x80, 0x01, 0xe0, 0x07, 0xe0, 0x07, 0x00, 0x00,
};
static const uint8_t icon_help[32] = {
    0x00, 0x00, 0xe0, 0x07, 0x38, 0x1c, 0x0c, 0x30,
    0xc4, 0x21, 0x46, 0x62, 0x02, 0x46, 0x02, 0x42,
    0x82, 0x43, 0x02, 0x40, 0x06, 0x60, 0x84, 0x21,
    0x0c, 0x30, 0x38, 0x1c, 0xe0, 0x07, 0x00, 0x00,
};
static const uint8_t icon_about[32] = {
    0x00, 0x00, 0xe0, 0x07, 0x38, 0x1c, 0x0c, 0x30,
    0x04, 0x20, 0x86, 0x61, 0x02, 0x40, 0x02, 0x40,
    0x82, 0x41, 0x82, 0x41, 0x86, 0x61, 0x04, 0x20,
    0x0c, 0x30, 0x38, 0x1c, 0xe0, 0x07, 0x00, 0x00,
};

static app_entry_t apps[] = {
    { "Terminal",   "Interactive shell",          launch_terminal,   icon_terminal },
    { "Browser",    "Tor Onion browser",         launch_browser,    icon_browser },
    { "Files",      "Disk & storage manager",     launch_files,      icon_files },
    { "Editor",     "Text editor & notepad",      launch_editor,     icon_editor },
    { "Calculator", "Interactive calculator",    launch_calculator, icon_calculator },
    { "Music",      "Sound studio & player",      launch_player,     icon_player },
    { "Settings",   "Display, DPI & control",     launch_settings,   icon_settings },
    { "System",     "Processes & memory",         launch_sysinfo,    icon_monitor },
    { "Help",       "Shortcuts & guidance",      launch_help,       icon_help },
    { "About",      "MyOS architecture",         launch_about,      icon_about },
};

#define APP_COUNT ((int)ARRAY_SIZE(apps))

int desktop_app_count(void) { return APP_COUNT; }

const app_entry_t *desktop_app_at(int i) {
    if (i < 0 || i >= APP_COUNT) return NULL;
    return &apps[i];
}

bool desktop_launch(const char *name) {
    if (!name) return false;
    for (int i = 0; i < APP_COUNT; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            apps[i].launch();
            desktop_invalidate();
            return true;
        }
    }
    if (strcmp(name, "tor") == 0 || strcmp(name, "Tor") == 0 ||
        strcmp(name, "Tor Browser") == 0 || strcmp(name, "browser") == 0) {
        launch_browser();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "terminal") == 0 || strcmp(name, "sh") == 0 || strcmp(name, "shell") == 0) {
        launch_terminal();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "files") == 0 || strcmp(name, "storage") == 0) {
        launch_files();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "calc") == 0 || strcmp(name, "calculator") == 0) {
        launch_calculator();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "editor") == 0 || strcmp(name, "notepad") == 0 || strcmp(name, "edit") == 0) {
        launch_editor();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "music") == 0 || strcmp(name, "player") == 0 || strcmp(name, "sound") == 0) {
        launch_player();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "settings") == 0 || strcmp(name, "control") == 0) {
        launch_settings();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "sysinfo") == 0 || strcmp(name, "system") == 0) {
        launch_sysinfo();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "calendar") == 0 || strcmp(name, "Calendar") == 0 || strcmp(name, "cal") == 0) {
        app_open_calendar();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "network") == 0 || strcmp(name, "Network") == 0 || strcmp(name, "net") == 0 || strcmp(name, "wifi") == 0) {
        app_open_network();
        desktop_invalidate();
        return true;
    }
    if (strcmp(name, "power") == 0 || strcmp(name, "Power") == 0 || strcmp(name, "battery") == 0) {
        app_open_power();
        desktop_invalidate();
        return true;
    }
    return false;
}

static bool app_is_running(const char *name) {
    int n = wm_window_count();
    for (int i = 0; i < n; i++) {
        wm_window_t *w = wm_window_by_creation(i);
        if (w && strstr(w->title, name)) return true;
    }
    return false;
}

/* ---- wallpaper ----------------------------------------------------------- */

static surface_t *wallpaper_surf;
static int current_theme_id = 0;

void desktop_set_theme(const char *name) {
    if (!name) return;
    if (strcmp(name, "emerald") == 0) current_theme_id = 1;
    else if (strcmp(name, "amber") == 0) current_theme_id = 2;
    else if (strcmp(name, "cyberpunk") == 0) current_theme_id = 3;
    else current_theme_id = 0;
    wallpaper_done = false;
    wm_invalidate_all();
}

/* Alpine mountain profile calculation */
static int mountain_profile(int x) {
    /* 12 anchor heights across 1024 width */
    static const int anchors[13] = {
        370, 310, 350, 280, 340, 250, 320, 270, 330, 290, 350, 320, 360
    };
    int seg = (x * 12) / (scr_w > 0 ? scr_w : 1024);
    if (seg < 0) seg = 0;
    if (seg > 11) seg = 11;
    int seg_w = scr_w / 12;
    if (seg_w <= 0) seg_w = 1;
    int rem = x - seg * seg_w;
    int y0 = anchors[seg];
    int y1 = anchors[seg + 1];
    int interp = y0 + ((y1 - y0) * rem) / seg_w;
    /* High-frequency jagged granite detail */
    int jag = ((x * 37 + 13) % 19) - 9;
    return interp + jag;
}

static void draw_wallpaper(void) {
    if (!wallpaper_surf) {
        wallpaper_surf = surface_create(scr_w, scr_h);
        if (!wallpaper_surf) return;
    }
    surface_t *s = wallpaper_surf;
    uint32_t *p = s->pixels;
    int stride = s->pitch / 4;

    int water_y = (scr_h * 55) / 100;
    int moon_cx = (scr_w * 28) / 100;
    int moon_cy = (scr_h * 18) / 100;
    int moon_rad = 22;

    /* 1. Sky Gradient (y = 0 .. water_y) */
    for (int y = 0; y < water_y; y++) {
        uint32_t t = (uint32_t)((uint64_t)y * 255 / (uint64_t)water_y);
        uint32_t r = 2 + (14 * t) / 255;
        uint32_t g = 4 + (24 * t) / 255;
        uint32_t b = 12 + (46 * t) / 255;
        if (current_theme_id == 1) { /* emerald */
            g += (40 * t) / 255; b -= (10 * t) / 255;
        } else if (current_theme_id == 2) { /* amber */
            r += (50 * t) / 255; g += (25 * t) / 255; b -= (10 * t) / 255;
        } else if (current_theme_id == 3) { /* cyberpunk */
            r += (35 * t) / 255; b += (30 * t) / 255;
        }
        uint32_t c = (r << 16) | (g << 8) | b;
        uint32_t *row = p + (size_t)y * (size_t)stride;
        for (int x = 0; x < scr_w; x++) row[x] = c;
    }

    /* 2. Moon & Celestial Aura */
    int aura_rad = 85;
    int aura_rad2 = aura_rad * aura_rad;
    int moon_rad2 = moon_rad * moon_rad;
    for (int dy = -aura_rad; dy <= aura_rad; dy++) {
        int y = moon_cy + dy;
        if (y < 0 || y >= water_y) continue;
        uint32_t *row = p + (size_t)y * (size_t)stride;
        for (int dx = -aura_rad; dx <= aura_rad; dx++) {
            int x = moon_cx + dx;
            if (x < 0 || x >= scr_w) continue;
            int d2 = dx * dx + dy * dy;
            if (d2 <= moon_rad2) {
                /* Radiant Lunar disc */
                int edge = moon_rad2 - d2;
                if (edge < moon_rad * 3) {
                    row[x] = RGB(225, 240, 255);
                } else {
                    row[x] = RGB(255, 255, 255);
                }
            } else if (d2 <= aura_rad2) {
                /* Atmospheric soft nebula glow */
                uint32_t factor = 255 - (d2 * 255 / aura_rad2);
                uint32_t t = (factor * factor) / 255;
                uint32_t cur = row[x];
                uint32_t cr = (cur >> 16) & 0xFF;
                uint32_t cg = (cur >> 8) & 0xFF;
                uint32_t cb = cur & 0xFF;
                cr += (45 * t) / 255;
                cg += (65 * t) / 255;
                cb += (95 * t) / 255;
                if (cr > 255) cr = 255;
                if (cg > 255) cg = 255;
                if (cb > 255) cb = 255;
                row[x] = (cr << 16) | (cg << 8) | cb;
            }
        }
    }

    /* 3. Starfield in the night sky */
    for (int i = 0; i < 150; i++) {
        int sx = (i * 211 + 53) % scr_w;
        int sy = (i * 137 + 29) % (water_y - 20);
        int ddx = sx - moon_cx, ddy = sy - moon_cy;
        if (ddx * ddx + ddy * ddy < aura_rad2) continue; /* Don't overwrite moon */

        uint32_t sc = (i % 7 == 0) ? RGB(255, 255, 255) :
                      ((i % 3 == 0) ? RGB(200, 225, 255) : RGB(140, 175, 220));
        p[(size_t)sy * (size_t)stride + sx] = sc;
        /* Twinkling 4-point cross glint for select bright stars */
        if (i % 12 == 0) {
            if (sx > 0) p[(size_t)sy * (size_t)stride + sx - 1] = RGB(160, 200, 245);
            if (sx < scr_w - 1) p[(size_t)sy * (size_t)stride + sx + 1] = RGB(160, 200, 245);
            if (sy > 0) p[(size_t)(sy - 1) * (size_t)stride + sx] = RGB(160, 200, 245);
            if (sy < water_y - 1) p[(size_t)(sy + 1) * (size_t)stride + sx] = RGB(160, 200, 245);
        }
    }

    /* 4. Alpine Mountain Ridge & Body */
    for (int x = 0; x < scr_w; x++) {
        int my = mountain_profile(x);
        if (my >= water_y) my = water_y - 1;

        /* Crest rim highlight */
        p[(size_t)my * (size_t)stride + x] = RGB(75, 170, 235);
        if (my + 1 < water_y) {
            p[(size_t)(my + 1) * (size_t)stride + x] = RGB(130, 95, 215);
        }

        /* Granite mountain body gradient down to water */
        for (int y = my + 2; y < water_y; y++) {
            uint32_t t = (uint32_t)((uint64_t)(y - my) * 255 / (uint64_t)(water_y - my));
            uint32_t mr = 8 + (6 * t) / 255;
            uint32_t mg = 12 + (8 * t) / 255;
            uint32_t mb = 22 + (12 * t) / 255;
            p[(size_t)y * (size_t)stride + x] = (mr << 16) | (mg << 8) | mb;
        }
    }

    /* 5. Alpine Lake with Moon & Mountain Reflection */
    for (int y = water_y; y < scr_h; y++) {
        int dy = y - water_y;
        int ref_y = water_y - 1 - (dy * 7) / 10;
        if (ref_y < 0) ref_y = 0;

        uint32_t *row = p + (size_t)y * (size_t)stride;
        for (int x = 0; x < scr_w; x++) {
            int wave_dx = ((y * 13) % 9) - 4;
            int rx = x + wave_dx;
            if (rx < 0) rx = 0;
            if (rx >= scr_w) rx = scr_w - 1;

            uint32_t ref_c = p[(size_t)ref_y * (size_t)stride + rx];
            uint32_t rr = (ref_c >> 16) & 0xFF;
            uint32_t rg = (ref_c >> 8) & 0xFF;
            uint32_t rb = ref_c & 0xFF;

            /* Attenuate reflection and blend with deep lake water */
            rr = (rr * 140) / 255 + 4;
            rg = (rg * 150) / 255 + 8;
            rb = (rb * 170) / 255 + 18;

            /* Moon vertical shimmer column */
            int mdx = x - moon_cx;
            if (mdx < 0) mdx = -mdx;
            if (mdx < 42) {
                int shim = (42 - mdx) * 255 / 42;
                int wave_mod = ((y * 7 + x * 3) % 11 < 6) ? 1 : 0;
                if (wave_mod) {
                    rr += (shim * 70) / 255;
                    rg += (shim * 95) / 255;
                    rb += (shim * 130) / 255;
                }
            }

            if (rr > 255) rr = 255;
            if (rg > 255) rg = 255;
            if (rb > 255) rb = 255;
            row[x] = (rr << 16) | (rg << 8) | rb;
        }
    }

    wallpaper_done = true;
}

static void blit_wallpaper(uint32_t *bb, int stride) {
    if (!wallpaper_surf) return;
    blit_copy_offset(bb, stride * 4, 0, 0,
                     wallpaper_surf->pixels, wallpaper_surf->pitch, 0, 0,
                     scr_w, scr_h);
}

/* ---- drawing helpers on the back buffer ---------------------------------- */

static inline void px(uint32_t *bb, int stride, int x, int y, color_t c) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) return;
    bb[(size_t)y * (size_t)stride + x] = c;
}

static void draw_rect_aa(uint32_t *bb, int stride, const rect_t *r, color_t c,
                         int alpha) {
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
    int r4 = radius * 4;
    int r4_sq = r4 * r4;
    int corners[4][2] = {
        { r->x + radius, r->y + radius },
        { r->x + r->w - radius - 1, r->y + radius },
        { r->x + radius, r->y + r->h - radius - 1 },
        { r->x + r->w - radius - 1, r->y + r->h - radius - 1 },
    };
    for (int k = 0; k < 4; k++) {
        int cx = corners[k][0], cy = corners[k][1];
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                int x = (k & 1) ? (cx + dx) : (cx - radius + 1 + dx);
                int y = (k & 2) ? (cy + dy) : (cy - radius + 1 + dy);
                if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) continue;

                int sx = (x - cx) * 4;
                int sy = (y - cy) * 4;
                int cov = 0;
                if ((sx + 1) * (sx + 1) + (sy + 1) * (sy + 1) <= r4_sq) cov++;
                if ((sx + 3) * (sx + 3) + (sy + 1) * (sy + 1) <= r4_sq) cov++;
                if ((sx + 1) * (sx + 1) + (sy + 3) * (sy + 3) <= r4_sq) cov++;
                if ((sx + 3) * (sx + 3) + (sy + 3) * (sy + 3) <= r4_sq) cov++;
                if (cov == 0) continue;

                int eff_alpha = (alpha * cov) / 4;
                if (eff_alpha >= 255) {
                    px(bb, stride, x, y, c);
                } else {
                    uint32_t d = bb[(size_t)y * (size_t)stride + x];
                    uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * eff_alpha) / 255;
                    uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * eff_alpha) / 255;
                    uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * eff_alpha) / 255;
                    bb[(size_t)y * (size_t)stride + x] = (r0 << 16) | (g0 << 8) | b0;
                }
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
        int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
        int cx = (q & 1) ? (r->x + r->w - radius - 1) : (r->x + radius);
        int cy = (q & 2) ? (r->y + r->h - radius - 1) : (r->y + radius);
        for (int dy = 0; dy <= radius; dy++) {
            for (int dx = 0; dx <= radius; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 > ro || d2 < ri) continue;
                for (int t = 0; t < thickness; t++) {
                    int px_x = cx + sx * (dx - t);
                    int px_y = cy + sy * (dy - t);
                    if (px_x < 0 || px_y < 0 || px_x >= scr_w || px_y >= scr_h) continue;
                    px(bb, stride, px_x, px_y, c);
                }
            }
        }
    }
}

static void draw_text_bb(uint32_t *bb, int stride, const baked_font_t *f,
                         int x, int y, const char *str, color_t c) {
    if (!bb || !f || !str) return;
    int pen = x;
    for (int i = 0; str[i]; ) {
        uint32_t cp;
        int n = utf8_decode(str + i, &cp);
        i += n;
        if (cp < (uint32_t)f->first_cp || cp >= (uint32_t)(f->first_cp + f->glyph_count))
            continue;
        int idx = (int)cp - f->first_cp;
        int gw = f->gw[idx], gh = f->gh[idx];
        int xo = f->xo[idx], yo = f->yo[idx];
        int xa = f->xa[idx];
        const uint8_t *bits = f->bitmap + f->bo[idx];
        int pitch_bits = (gw + 7) / 8;
        for (int r = 0; r < gh; r++) {
            int py = y + yo + r;
            if (py < 0 || py >= scr_h) continue;
            for (int col = 0; col < gw; col++) {
                int px_x = pen + xo + col;
                if (px_x < 0 || px_x >= scr_w) continue;
                if (bits[r * pitch_bits + (col >> 3)] & (1 << (col & 7))) {
                    px(bb, stride, px_x, py, c);
                }
            }
        }
        pen += xa;
    }
}

/* ---- clock --------------------------------------------------------------- */

void desktop_format_clock(char *buf, int len, bool with_seconds) {
    datetime_t t;
    rtc_get_time(&t);
    if (with_seconds) {
        snprintf(buf, len, "%02u:%02u:%02u", t.hour, t.minute, t.second);
    } else {
        snprintf(buf, len, "%02u:%02u", t.hour, t.minute);
    }
}

/* ---- App tiles for menus and launchers ----------------------------------- */

static void draw_app_tile(uint32_t *bb, int stride, int x, int y, int size, int app_idx) {
    rect_t r = { x, y, size, size };
    rect_t sh = { x + 1, y + 2, size, size };
    draw_round_rect(bb, stride, &sh, 10, RGB(0, 0, 0), 120);

    color_t bg_top, bg_bot, border_col;
    switch (app_idx) {
        case 0: /* Terminal */
            bg_top = RGB(0x1E, 0x24, 0x2F); bg_bot = RGB(0x10, 0x14, 0x1C);
            border_col = RGB(0x38, 0x44, 0x56);
            break;
        case 1: /* Tor Browser */
            bg_top = RGB(0x1A, 0x8A, 0xCC); bg_bot = RGB(0x0E, 0x4B, 0x80);
            border_col = RGB(0x40, 0xB5, 0xF5);
            break;
        case 2: /* Files */
            bg_top = RGB(0x2B, 0x7E, 0xF5); bg_bot = RGB(0x18, 0x54, 0xBA);
            border_col = RGB(0x5E, 0x9D, 0xF8);
            break;
        case 3: /* System */
            bg_top = RGB(0x2E, 0xB0, 0x55); bg_bot = RGB(0x1A, 0x72, 0x33);
            border_col = RGB(0x4C, 0xD2, 0x76);
            break;
        case 4: /* Help */
            bg_top = RGB(0xF0, 0xAC, 0x2E); bg_bot = RGB(0xB8, 0x74, 0x12);
            border_col = RGB(0xFA, 0xC4, 0x5A);
            break;
        case 5: /* About */
        default:
            bg_top = RGB(0x94, 0x5E, 0xFA); bg_bot = RGB(0x64, 0x31, 0xC4);
            border_col = RGB(0xAF, 0x83, 0xFB);
            break;
    }

    draw_round_rect(bb, stride, &r, 10, bg_top, 255);
    rect_t bot_half = { x, y + size / 2, size, size - size / 2 };
    draw_round_rect(bb, stride, &bot_half, 10, bg_bot, 160);
    draw_round_outline(bb, stride, &r, 10, border_col, 1, 230);

    int cx = x + size / 2;
    int cy = y + size / 2;

    if (app_idx == 0) { /* Terminal >_ */
        color_t ch_col = RGB(0x39, 0xD3, 0x53);
        for (int i = 0; i <= 5; i++) {
            px(bb, stride, cx - 8 + i, cy - 6 + i, ch_col);
            px(bb, stride, cx - 8 + i, cy - 6 + i + 1, ch_col);
            px(bb, stride, cx - 3 - i, cy + i, ch_col);
            px(bb, stride, cx - 3 - i, cy + i + 1, ch_col);
        }
        color_t cur_col = RGB(0x58, 0xA6, 0xFF);
        for (int i = 0; i < 7; i++) {
            px(bb, stride, cx + 1 + i, cy + 4, cur_col);
            px(bb, stride, cx + 1 + i, cy + 5, cur_col);
        }
    } else if (app_idx == 1) { /* Tor Browser Globe */
        color_t gl_col = RGB(0xFF, 0xFF, 0xFF);
        rect_t disc = { cx - 9, cy - 9, 18, 18 };
        draw_round_rect(bb, stride, &disc, 9, gl_col, 255);
        rect_t inner = { cx - 7, cy - 7, 14, 14 };
        draw_round_rect(bb, stride, &inner, 7, bg_top, 255);
        for (int i = -6; i <= 6; i++) {
            px(bb, stride, cx + i, cy, RGB(0x80, 0xD0, 0xFF));
            px(bb, stride, cx, cy + i, RGB(0x80, 0xD0, 0xFF));
        }
        rect_t shld = { cx - 3, cy - 3, 6, 6 };
        draw_round_rect(bb, stride, &shld, 2, RGB(0xBD, 0x53, 0xED), 255);
    } else if (app_idx == 2) { /* Files Folder */
        color_t fld_col = RGB(0xFF, 0xFF, 0xFF);
        rect_t tab = { cx - 10, cy - 8, 9, 4 };
        draw_round_rect(bb, stride, &tab, 2, fld_col, 255);
        rect_t body = { cx - 11, cy - 5, 22, 14 };
        draw_round_rect(bb, stride, &body, 3, fld_col, 255);
        rect_t inner = { cx - 9, cy - 3, 18, 10 };
        draw_round_rect(bb, stride, &inner, 2, RGB(0x2B, 0x7E, 0xF5), 230);
    } else if (app_idx == 3) { /* System */
        color_t ekg_col = RGB(0xFF, 0xFF, 0xFF);
        for (int i = -10; i <= -5; i++) { px(bb, stride, cx + i, cy, ekg_col); px(bb, stride, cx + i, cy + 1, ekg_col); }
        for (int i = 0; i <= 7; i++) { px(bb, stride, cx - 5 + i / 2, cy - i, ekg_col); px(bb, stride, cx - 5 + i / 2, cy - i + 1, ekg_col); }
        for (int i = 0; i <= 12; i++) { px(bb, stride, cx - 1 + i / 3, cy - 7 + i, ekg_col); px(bb, stride, cx - 1 + i / 3, cy - 7 + i + 1, ekg_col); }
        for (int i = 3; i <= 10; i++) { px(bb, stride, cx + i, cy, ekg_col); px(bb, stride, cx + i, cy + 1, ekg_col); }
    } else if (app_idx == 4) { /* Help */
        color_t q_col = RGB(0xFF, 0xFF, 0xFF);
        rect_t disc = { cx - 9, cy - 9, 18, 18 };
        draw_round_rect(bb, stride, &disc, 9, q_col, 255);
        rect_t inner = { cx - 7, cy - 7, 14, 14 };
        draw_round_rect(bb, stride, &inner, 7, bg_top, 255);
        draw_text_bb(bb, stride, font_bold(), cx - 3, cy - 7, "?", q_col);
    } else { /* About */
        logo_draw_buffer(bb, stride, scr_w, scr_h, cx, cy, 22);
    }
}

/* ---- Top Status Bar ----------------------------------------------------- */

static void draw_top_bar(uint32_t *bb, int stride) {
    rect_t bar = { 0, 0, scr_w, topbar_h };
    draw_rect_aa(bb, stride, &bar, RGB(10, 14, 22), 235);
    rect_t edge = { 0, topbar_h - 1, scr_w, 1 };
    draw_rect_aa(bb, stride, &edge, RGB(35, 48, 68), 255);

    /* Left: Brand typography + Active focus breadcrumb */
    draw_text_bb(bb, stride, font_bold(), 14, 6, "MyOS", RGB(255, 255, 255));
    draw_text_bb(bb, stride, font_ui(), 54, 6, "|", RGB(70, 85, 110));
    wm_window_t *fw = wm_focused();
    const char *status_str = (fw && fw->title[0]) ? fw->title : "Desktop";
    draw_text_bb(bb, stride, font_ui(), 66, 6, status_str, RGB(160, 185, 220));

    /* Right System Tray: Wi-Fi, Volume, Battery, Clock, User Profile */
    int cur_rx = scr_w - 18;

    /* User Profile Card */
    const char *cur_u = auth_get_current_user();
    char ubadge[AUTH_NAME_MAX + 2];
    ubadge[0] = '@';
    ui_strcpy(ubadge + 1, sizeof(ubadge) - 1, (cur_u && cur_u[0]) ? cur_u : "myos");
    int ubw = text_width(font_bold(), ubadge);
    cur_rx -= (ubw + 12);
    rect_t ucard = { cur_rx, 3, ubw + 10, topbar_h - 6 };
    draw_round_rect(bb, stride, &ucard, 4, RGB(22, 32, 48), 220);
    draw_round_outline(bb, stride, &ucard, 4, RGB(45, 65, 95), 1, 230);
    draw_text_bb(bb, stride, font_bold(), cur_rx + 5, 6, ubadge, RGB(120, 175, 255));

    power_state_t pwr;
    power_get_state(&pwr);
    net_state_t net;
    net_get_state(&net);

    /* Digital Clock (HH:MM:SS) */
    if (last_clock_s == 0) desktop_format_clock(clock_buf, sizeof(clock_buf), true);
    int cw = text_width(font_mono(), clock_buf);
    cur_rx -= (cw + 14);
    hit_clock_x1 = cur_rx + cw + 10;
    hit_clock_x0 = cur_rx - 4;
    draw_text_bb(bb, stride, font_mono(), cur_rx, 6, clock_buf, RGB(235, 245, 255));

    /* Battery Icon & Percentage */
    char btext[16];
    snprintf(btext, sizeof(btext), "%u%%", pwr.battery_percent);
    int btw = text_width(font_ui(), btext);
    cur_rx -= (btw + 28);
    hit_batt_x1 = hit_clock_x0;
    hit_batt_x0 = cur_rx - 4;

    /* Battery Text */
    draw_text_bb(bb, stride, font_ui(), cur_rx, 6, btext, pwr.is_charging ? RGB(70, 225, 130) : RGB(200, 215, 235));

    /* Battery Graphic Body */
    int bg_x = cur_rx + btw + 4;
    rect_t batt_body = { bg_x, 8, 18, 10 };
    draw_round_outline(bb, stride, &batt_body, 3, RGB(130, 150, 180), 1, 255);
    int fill_w = (14 * (int)pwr.battery_percent) / 100;
    if (fill_w < 2) fill_w = 2;
    rect_t batt_fill = { bg_x + 2, 10, fill_w, 6 };
    color_t bcolor = pwr.is_charging ? RGB(46, 220, 110)
                   : (pwr.battery_percent > 35 ? RGB(46, 200, 105) : (pwr.battery_percent > 15 ? RGB(235, 175, 40) : RGB(235, 60, 60)));
    draw_round_rect(bb, stride, &batt_fill, 1, bcolor, 255);
    rect_t batt_nub = { bg_x + 18, 11, 2, 4 };
    draw_round_rect(bb, stride, &batt_nub, 1, RGB(130, 150, 180), 255);

    /* If Charging, draw yellow lightning bolt symbol inside/above battery */
    if (pwr.is_charging) {
        color_t bolt_col = RGB(255, 220, 50);
        px(bb, stride, bg_x + 8, 8, bolt_col);
        px(bb, stride, bg_x + 7, 9, bolt_col);
        px(bb, stride, bg_x + 8, 10, bolt_col);
        px(bb, stride, bg_x + 9, 10, bolt_col);
        px(bb, stride, bg_x + 10, 10, bolt_col);
        px(bb, stride, bg_x + 8, 11, bolt_col);
        px(bb, stride, bg_x + 7, 12, bolt_col);
        px(bb, stride, bg_x + 8, 13, bolt_col);
    }

    /* Volume Speaker Icon */
    cur_rx -= 20;
    int spk_x = cur_rx, spk_y = 7;
    rect_t spk_box = { spk_x, spk_y + 3, 4, 6 };
    draw_rect_aa(bb, stride, &spk_box, RGB(210, 225, 245), 255);
    for (int i = 0; i < 5; i++) {
        px(bb, stride, spk_x + 4 + i, spk_y + 3 - i, RGB(210, 225, 245));
        px(bb, stride, spk_x + 4 + i, spk_y + 8 + i, RGB(210, 225, 245));
    }
    px(bb, stride, spk_x + 11, spk_y + 4, RGB(210, 225, 245));
    px(bb, stride, spk_x + 12, spk_y + 5, RGB(210, 225, 245));
    px(bb, stride, spk_x + 12, spk_y + 6, RGB(210, 225, 245));
    px(bb, stride, spk_x + 11, spk_y + 7, RGB(210, 225, 245));

    /* Bluetooth Icon */
    cur_rx -= 18;
    int bt_x = cur_rx + 4, bt_y = 8;
    color_t bt_col = net.bt_enabled ? RGB(75, 165, 255) : RGB(85, 100, 125);
    for (int y = 0; y < 11; y++) px(bb, stride, bt_x + 4, bt_y + y, bt_col);
    px(bb, stride, bt_x + 5, bt_y + 1, bt_col);
    px(bb, stride, bt_x + 6, bt_y + 2, bt_col);
    px(bb, stride, bt_x + 7, bt_y + 3, bt_col);
    px(bb, stride, bt_x + 6, bt_y + 4, bt_col);
    px(bb, stride, bt_x + 5, bt_y + 5, bt_col);
    px(bb, stride, bt_x + 6, bt_y + 6, bt_col);
    px(bb, stride, bt_x + 7, bt_y + 7, bt_col);
    px(bb, stride, bt_x + 6, bt_y + 8, bt_col);
    px(bb, stride, bt_x + 5, bt_y + 9, bt_col);
    px(bb, stride, bt_x + 3, bt_y + 2, bt_col);
    px(bb, stride, bt_x + 2, bt_y + 1, bt_col);
    px(bb, stride, bt_x + 3, bt_y + 8, bt_col);
    px(bb, stride, bt_x + 2, bt_y + 9, bt_col);

    /* Wi-Fi Signal Arcs */
    cur_rx -= 20;
    int w_cx = cur_rx + 8, w_cy = 17;
    color_t wifi_col = (net.wifi_enabled && net.wifi_connected) ? RGB(55, 215, 150) : RGB(85, 100, 125);
    px(bb, stride, w_cx, w_cy, wifi_col);
    for (int dx = -3; dx <= 3; dx++) {
        if (dx == -3 || dx == 3) px(bb, stride, w_cx + dx, w_cy - 3, wifi_col);
        else if (dx >= -2 && dx <= 2) px(bb, stride, w_cx + dx, w_cy - 4, wifi_col);
    }
    for (int dx = -6; dx <= 6; dx++) {
        if (dx == -6 || dx == 6) px(bb, stride, w_cx + dx, w_cy - 6, wifi_col);
        else if (dx >= -4 && dx <= 4) px(bb, stride, w_cx + dx, w_cy - 8, wifi_col);
    }

    /* Ethernet LAN Icon */
    cur_rx -= 20;
    hit_net_x1 = bt_x + 12;
    hit_net_x0 = cur_rx - 4;
    int eth_x = cur_rx + 2, eth_y = 8;
    color_t eth_col = net.eth_connected ? RGB(50, 215, 130) : RGB(85, 100, 125);
    rect_t eth_box = { eth_x, eth_y, 11, 8 };
    draw_round_outline(bb, stride, &eth_box, 2, eth_col, 1, 255);
    px(bb, stride, eth_x + 5, eth_y + 8, eth_col);
    for (int k = 2; k <= 8; k++) px(bb, stride, eth_x + k, eth_y + 9, eth_col);
    if (net.eth_connected) px(bb, stride, eth_x + 5, eth_y + 4, RGB(80, 255, 160));
}

/* ---- Left Vertical Navigation Strip ------------------------------------- */

static void draw_left_nav(uint32_t *bb, int stride) {
    rect_t bar = { lnav_x, lnav_y, lnav_w, lnav_h };
    draw_round_rect(bb, stride, &bar, 14, RGB(13, 17, 26), 215);
    draw_round_outline(bb, stride, &bar, 14, RGB(38, 52, 76), 1, 230);

    /* 6 Nav Slots: Home, Apps, Terminal, Files, Tor Browser, Settings */
    int slot_h = lnav_h / 6;
    for (int i = 0; i < 6; i++) {
        int iy = lnav_y + i * slot_h;
        int cx = lnav_x + lnav_w / 2;
        int cy = iy + slot_h / 2;

        if (lnav_hot == i) {
            rect_t slot_r = { lnav_x + 4, iy + 4, lnav_w - 8, slot_h - 8 };
            draw_round_rect(bb, stride, &slot_r, 8, RGB(40, 60, 95), 180);
            draw_round_outline(bb, stride, &slot_r, 8, RGB(80, 130, 205), 1, 200);
        }

        switch (i) {
            case 0: { /* Home (Little House) */
                color_t hc = RGB(220, 235, 255);
                for (int d = 0; d <= 7; d++) {
                    px(bb, stride, cx - d, cy - 2 + d, hc);
                    px(bb, stride, cx + d, cy - 2 + d, hc);
                }
                rect_t wll = { cx - 5, cy + 5, 11, 7 };
                draw_round_rect(bb, stride, &wll, 2, hc, 255);
                break;
            }
            case 1: { /* Apps (Grid) */
                color_t ac = menu_open ? RGB(75, 175, 255) : RGB(200, 220, 245);
                for (int gy = -5; gy <= 5; gy += 5) {
                    for (int gx = -5; gx <= 5; gx += 5) {
                        rect_t dot = { cx + gx - 1, cy + gy - 1, 3, 3 };
                        draw_round_rect(bb, stride, &dot, 1, ac, 255);
                    }
                }
                break;
            }
            case 2: { /* Terminal (>_) */
                color_t tc = RGB(55, 215, 115);
                for (int d = 0; d <= 4; d++) {
                    px(bb, stride, cx - 5 + d, cy - 4 + d, tc);
                    px(bb, stride, cx - 1 - d, cy + d, tc);
                }
                for (int d = 0; d < 5; d++) px(bb, stride, cx + 1 + d, cy + 4, RGB(85, 165, 255));
                break;
            }
            case 3: { /* Files (Folder) */
                rect_t fbody = { cx - 7, cy - 4, 14, 10 };
                draw_round_rect(bb, stride, &fbody, 2, RGB(43, 126, 245), 255);
                rect_t ftab = { cx - 7, cy - 7, 7, 3 };
                draw_round_rect(bb, stride, &ftab, 1, RGB(43, 126, 245), 255);
                break;
            }
            case 4: { /* Tor Browser (Shield/Globe) */
                rect_t disc = { cx - 7, cy - 7, 14, 14 };
                draw_round_rect(bb, stride, &disc, 7, RGB(35, 155, 230), 255);
                rect_t inn = { cx - 5, cy - 5, 10, 10 };
                draw_round_rect(bb, stride, &inn, 5, RGB(16, 24, 38), 255);
                rect_t dot = { cx - 2, cy - 2, 4, 4 };
                draw_round_rect(bb, stride, &dot, 2, RGB(189, 83, 237), 255);
                break;
            }
            case 5: { /* Settings (Gear) */
                rect_t gear = { cx - 6, cy - 6, 12, 12 };
                draw_round_rect(bb, stride, &gear, 6, RGB(180, 195, 215), 255);
                rect_t hole = { cx - 3, cy - 3, 6, 6 };
                draw_round_rect(bb, stride, &hole, 3, RGB(14, 18, 28), 255);
                for (int g = -8; g <= 8; g += 8) {
                    px(bb, stride, cx + g, cy, RGB(180, 195, 215));
                    px(bb, stride, cx, cy + g, RGB(180, 195, 215));
                }
                break;
            }
        }
    }
}

/* ---- Floating Bottom Dock ----------------------------------------------- */

static void draw_dock(uint32_t *bb, int stride) {
    rect_t dock = { dock_x, dock_y, dock_w, dock_h };

    /* Drop shadow */
    rect_t sh = { dock_x + 2, dock_y + 4, dock_w, dock_h };
    draw_round_rect(bb, stride, &sh, 24, RGB(0, 0, 0), 140);

    /* Glassmorphic body with specular refraction line */
    draw_round_rect(bb, stride, &dock, 24, RGB(14, 18, 28), 235);
    draw_round_outline(bb, stride, &dock, 24, RGB(50, 75, 115), 1, 230);
    rect_t spec = { dock_x + 24, dock_y + 1, dock_w - 48, 1 };
    draw_rect_aa(bb, stride, &spec, RGB(130, 180, 255), 70);

    int slot_w = dock_w / 7;
    for (int i = 0; i < 7; i++) {
        int cx = dock_x + slot_w / 2 + i * slot_w;
        int cy = dock_y + dock_h / 2;

        /* Hover indicator pill */
        if (dock_hot == i) {
            rect_t hov = { cx - 20, cy - 20, 40, 40 };
            draw_round_rect(bb, stride, &hov, 12, RGB(255, 255, 255), 35);
            draw_round_outline(bb, stride, &hov, 12, RGB(90, 150, 240), 1, 160);
        }

        switch (i) {
            case 0: { /* Start Button: Signature Orbital 'M' 3D Logo */
                logo_draw_buffer(bb, stride, scr_w, scr_h, cx, cy - 1, 26);
                if (menu_open) {
                    rect_t ring = { cx - 18, cy - 19, 36, 36 };
                    draw_round_outline(bb, stride, &ring, 18, RGB(80, 175, 255), 2, 230);
                }
                break;
            }
            case 1: { /* File Manager */
                rect_t fld = { cx - 12, cy - 9, 24, 17 };
                draw_round_rect(bb, stride, &fld, 3, RGB(43, 126, 245), 255);
                rect_t tab = { cx - 12, cy - 13, 11, 5 };
                draw_round_rect(bb, stride, &tab, 2, RGB(43, 126, 245), 255);
                rect_t front = { cx - 10, cy - 6, 20, 12 };
                draw_round_rect(bb, stride, &front, 2, RGB(70, 155, 255), 230);
                if (app_is_running("Files")) {
                    rect_t ind = { cx - 2, dock_y + dock_h - 5, 4, 3 };
                    draw_round_rect(bb, stride, &ind, 1, RGB(70, 170, 255), 255);
                }
                break;
            }
            case 2: { /* Terminal */
                rect_t tbox = { cx - 12, cy - 10, 24, 19 };
                draw_round_rect(bb, stride, &tbox, 4, RGB(22, 28, 38), 255);
                draw_round_outline(bb, stride, &tbox, 4, RGB(60, 75, 100), 1, 255);
                for (int d = 0; d <= 4; d++) {
                    px(bb, stride, cx - 8 + d, cy - 5 + d, RGB(55, 215, 115));
                    px(bb, stride, cx - 4 - d, cy - 1 + d, RGB(55, 215, 115));
                }
                for (int d = 0; d < 6; d++) px(bb, stride, cx - 1 + d, cy + 3, RGB(85, 165, 255));
                if (app_is_running("Terminal")) {
                    rect_t ind = { cx - 2, dock_y + dock_h - 5, 4, 3 };
                    draw_round_rect(bb, stride, &ind, 1, RGB(55, 215, 115), 255);
                }
                break;
            }
            case 3: { /* Tor Browser */
                rect_t gbody = { cx - 12, cy - 12, 24, 24 };
                draw_round_rect(bb, stride, &gbody, 12, RGB(26, 138, 204), 255);
                draw_round_outline(bb, stride, &gbody, 12, RGB(75, 195, 255), 1, 255);
                for (int d = -8; d <= 8; d++) {
                    px(bb, stride, cx + d, cy, RGB(200, 240, 255));
                    px(bb, stride, cx, cy + d, RGB(200, 240, 255));
                }
                rect_t onion = { cx - 4, cy - 4, 8, 8 };
                draw_round_rect(bb, stride, &onion, 4, RGB(189, 83, 237), 255);
                if (app_is_running("Browser") || app_is_running("Tor")) {
                    rect_t ind = { cx - 2, dock_y + dock_h - 5, 4, 3 };
                    draw_round_rect(bb, stride, &ind, 1, RGB(189, 83, 237), 255);
                }
                break;
            }
            case 4: { /* Settings / System */
                rect_t sbox = { cx - 12, cy - 11, 24, 21 };
                draw_round_rect(bb, stride, &sbox, 4, RGB(35, 44, 60), 255);
                draw_round_outline(bb, stride, &sbox, 4, RGB(70, 90, 120), 1, 255);
                for (int d = -6; d <= -2; d++) px(bb, stride, cx + d, cy + 2, RGB(46, 200, 105));
                for (int d = 0; d <= 4; d++) px(bb, stride, cx - 2 + d / 2, cy - d, RGB(46, 200, 105));
                for (int d = 0; d <= 7; d++) px(bb, stride, cx + d / 2, cy - 4 + d, RGB(46, 200, 105));
                for (int d = 4; d <= 7; d++) px(bb, stride, cx + d, cy + 2, RGB(46, 200, 105));
                if (app_is_running("System")) {
                    rect_t ind = { cx - 2, dock_y + dock_h - 5, 4, 3 };
                    draw_round_rect(bb, stride, &ind, 1, RGB(46, 200, 105), 255);
                }
                break;
            }
            case 5: { /* Gallery / Photos */
                rect_t pframe = { cx - 12, cy - 10, 24, 19 };
                draw_round_rect(bb, stride, &pframe, 3, RGB(220, 230, 245), 255);
                rect_t pinner = { cx - 10, cy - 8, 20, 15 };
                draw_round_rect(bb, stride, &pinner, 2, RGB(40, 80, 150), 255);
                for (int d = 0; d <= 5; d++) {
                    px(bb, stride, cx - 5 + d, cy + 5 - d, RGB(230, 160, 60));
                    px(bb, stride, cx + d, cy + 5 - d, RGB(245, 190, 80));
                }
                rect_t sun = { cx + 4, cy - 5, 4, 4 };
                draw_round_rect(bb, stride, &sun, 2, RGB(255, 230, 100), 255);
                if (app_is_running("About")) {
                    rect_t ind = { cx - 2, dock_y + dock_h - 5, 4, 3 };
                    draw_round_rect(bb, stride, &ind, 1, RGB(245, 190, 80), 255);
                }
                break;
            }
            case 6: { /* Trash */
                rect_t tcan = { cx - 8, cy - 6, 16, 16 };
                draw_round_rect(bb, stride, &tcan, 2, RGB(140, 155, 175), 255);
                rect_t tlid = { cx - 10, cy - 9, 20, 3 };
                draw_round_rect(bb, stride, &tlid, 1, RGB(180, 195, 215), 255);
                rect_t thand = { cx - 3, cy - 11, 6, 2 };
                draw_round_rect(bb, stride, &thand, 1, RGB(180, 195, 215), 255);
                for (int l = -4; l <= 4; l += 4) {
                    for (int dy = -3; dy <= 5; dy++) px(bb, stride, cx + l, cy + dy, RGB(80, 95, 115));
                }
                break;
            }
        }
    }
}

/* ---- Start Menu Flyout -------------------------------------------------- */

static void draw_menu(uint32_t *bb, int stride) {
    if (!menu_open) return;
    int mw = 310, mh = 380;
    int mx = (scr_w - mw) / 2;
    int my = dock_y - mh - 12;
    rect_t card = { mx, my, mw, mh };

    /* Drop shadow */
    rect_t sh = { mx + 3, my + 6, mw, mh };
    draw_round_rect(bb, stride, &sh, 16, RGB(0, 0, 0), 150);
    draw_round_rect(bb, stride, &card, 16, RGB(15, 20, 30), 248);
    draw_round_outline(bb, stride, &card, 16, RGB(55, 80, 125), 1, 255);

    /* Search Bar */
    rect_t sbar = { mx + 14, my + 14, mw - 28, 34 };
    draw_round_rect(bb, stride, &sbar, 8, RGB(24, 32, 46), 255);
    draw_round_outline(bb, stride, &sbar, 8, RGB(48, 66, 96), 1, 230);
    draw_text_bb(bb, stride, font_ui(), mx + 26, my + 23, "Search apps & storage...", RGB(120, 145, 180));

    /* App Items List */
    for (int i = 0; i < APP_COUNT; i++) {
        int iy = my + 58 + i * 46;
        rect_t row = { mx + 10, iy, mw - 20, 42 };
        if (menu_hot == i) {
            draw_round_rect(bb, stride, &row, 8, TH_ACCENT_SOFT, 255);
            draw_round_outline(bb, stride, &row, 8, TH_BORDER_FOCUS, 1, 220);
        }
        draw_app_tile(bb, stride, mx + 18, iy + 5, 32, i);
        draw_text_bb(bb, stride, font_bold(), mx + 60, iy + 6, apps[i].name, TH_TEXT_BRIGHT);
        draw_text_bb(bb, stride, font_ui(), mx + 60, iy + 22, apps[i].desc, TH_TEXT_DIM);
    }

    /* Footer: User profile & Lock button */
    rect_t footer = { mx + 10, my + mh - 44, mw - 20, 36 };
    draw_round_rect(bb, stride, &footer, 8, RGB(22, 28, 40), 220);
    draw_round_outline(bb, stride, &footer, 8, RGB(42, 54, 76), 1, 200);

    const char *cur_u = auth_get_current_user();
    char ubadge[AUTH_NAME_MAX + 2];
    ubadge[0] = '@';
    ui_strcpy(ubadge + 1, sizeof(ubadge) - 1, (cur_u && cur_u[0]) ? cur_u : "myos");
    draw_text_bb(bb, stride, font_bold(), mx + 20, my + mh - 32, ubadge, RGB(120, 175, 255));

    rect_t lock_btn = { mx + mw - 76, my + mh - 40, 58, 28 };
    draw_round_rect(bb, stride, &lock_btn, 6, RGB(35, 48, 70), 255);
    draw_round_outline(bb, stride, &lock_btn, 6, RGB(65, 90, 130), 1, 220);
    draw_text_bb(bb, stride, font_ui(), mx + mw - 62, my + mh - 33, "Lock", RGB(220, 235, 255));
}

/* ---- Desktop shortcuts -------------------------------------------------- */

static void draw_desktop_icons(uint32_t *bb, int stride) {
    for (int i = 0; i < icon_count; i++) {
        icon_slot_t *s = &icons[i];
        rect_t plate = { s->x, s->y, 88, 86 };
        if (s->hot) {
            draw_round_rect(bb, stride, &plate, 10, TH_ACCENT_SOFT, 160);
            draw_round_outline(bb, stride, &plate, 10, TH_BORDER_FOCUS, 1, 200);
        }
        draw_app_tile(bb, stride, s->x + 24, s->y + 6, 40, i);
        const char *nm = s->app->name;
        int tw = text_width(font_bold(), nm);
        draw_text_bb(bb, stride, font_bold(), s->x + (88 - tw) / 2, s->y + 50, nm, TH_TEXT_BRIGHT);
        int dw = text_width(font_ui(), s->app->desc);
        if (dw < 88)
            draw_text_bb(bb, stride, font_ui(), s->x + (88 - dw) / 2, s->y + 66,
                         s->app->desc, TH_TEXT_DIM);
    }
}

/* ---- init & lifecycle ---------------------------------------------------- */

void desktop_init(int w, int h) {
    scr_w = w;
    scr_h = h;
    workarea_bottom = h - 64;

    dock_w = 380;
    dock_h = 48;
    dock_x = (scr_w - dock_w) / 2;
    dock_y = scr_h - 58;

    lnav_w = 46;
    lnav_h = 260;
    lnav_x = 8;
    lnav_y = 36;

    menu_open = false;
    menu_hot = -1;
    dock_hot = -1;
    lnav_hot = -1;
    wallpaper_done = false;
    icon_count = 0;

    auth_init();
    term_register_service();
    desktop_format_clock(clock_buf, sizeof(clock_buf), false);
    last_clock_s = 1;
}

void desktop_shutdown(void) { wm_shutdown(); }

void desktop_invalidate(void) { wallpaper_done = false; }

void desktop_invalidate_rect(const rect_t *r) {
    if (r) fb_add_damage(r->x, r->y, r->w, r->h);
}

/* ---- hit testing & interactions ------------------------------------------ */

static bool in_dock(int x, int y, int *slot) {
    if (x >= dock_x && x < dock_x + dock_w && y >= dock_y && y < dock_y + dock_h) {
        if (slot) *slot = (x - dock_x) * 7 / dock_w;
        return true;
    }
    return false;
}

static bool in_left_nav(int x, int y, int *slot) {
    if (x >= lnav_x && x < lnav_x + lnav_w && y >= lnav_y && y < lnav_y + lnav_h) {
        if (slot) *slot = (y - lnav_y) * 6 / lnav_h;
        return true;
    }
    return false;
}

static int menu_index_at(int x, int y) {
    if (!menu_open) return -1;
    int mw = 310, mh = 380;
    int mx = (scr_w - mw) / 2;
    int my = dock_y - mh - 12;
    for (int i = 0; i < APP_COUNT; i++) {
        rect_t row = { mx + 10, my + 58 + i * 46, mw - 20, 42 };
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
        /* Check lock button in start menu */
        int mw = 310, mh = 380;
        int mx = (scr_w - mw) / 2;
        int my = dock_y - mh - 12;
        rect_t lock_btn = { mx + mw - 76, my + mh - 40, 58, 28 };
        if (rect_contains_point(&lock_btn, x, y)) {
            menu_open = false;
            login_lock();
            return true;
        }
        /* Clicking outside the menu dismisses it */
        rect_t card = { mx, my, mw, mh };
        if (!rect_contains_point(&card, x, y)) {
            menu_open = false;
            return true;
        }
        return true;
    }

    /* Top bar clicks */
    if (y < topbar_h) {
        /* User profile click locks screen */
        if (x >= scr_w - 70) {
            login_lock();
            return true;
        }
        /* Clock click opens Calendar */
        if (x >= hit_clock_x0 && x < hit_clock_x1) {
            app_open_calendar();
            return true;
        }
        /* Battery click opens Power & Battery Manager */
        if (x >= hit_batt_x0 && x < hit_batt_x1) {
            app_open_power();
            return true;
        }
        /* Network click (WiFi, BT, Ethernet) opens Network Connections */
        if (x >= hit_net_x0 && x < hit_net_x1) {
            app_open_network();
            return true;
        }
        return true;
    }

    /* Left navigation bar clicks */
    int lslot;
    if (in_left_nav(x, y, &lslot)) {
        switch (lslot) {
            case 0: wm_minimize_all(); break;
            case 1: menu_open = !menu_open; break;
            case 2: launch_terminal(); break;
            case 3: launch_files(); break;
            case 4: launch_browser(); break;
            case 5: launch_sysinfo(); break;
        }
        return true;
    }

    /* Bottom dock clicks */
    int dslot;
    if (in_dock(x, y, &dslot)) {
        switch (dslot) {
            case 0: menu_open = !menu_open; break;
            case 1: launch_files(); break;
            case 2: launch_terminal(); break;
            case 3: launch_browser(); break;
            case 4: launch_sysinfo(); break;
            case 5: launch_about(); break;
            case 6: launch_help(); break;
        }
        return true;
    }

    /* Desktop icon double-click or click opens the app */
    for (int i = 0; i < icon_count; i++) {
        rect_t r = { icons[i].x, icons[i].y, 88, 86 };
        if (rect_contains_point(&r, x, y)) {
            desktop_launch(icons[i].app->name);
            return true;
        }
    }
    return false;
}

static void desktop_hover(int x, int y) {
    int ds;
    dock_hot = in_dock(x, y, &ds) ? ds : -1;
    int ls;
    lnav_hot = in_left_nav(x, y, &ls) ? ls : -1;
    menu_hot = menu_index_at(x, y);
    for (int i = 0; i < icon_count; i++) {
        rect_t r = { icons[i].x, icons[i].y, 88, 86 };
        icons[i].hot = rect_contains_point(&r, x, y);
    }
}

/* ---- frame rendering ---------------------------------------------------- */

void desktop_tick(void) {
    input_pump();

    /* If system is locked, route all interaction to the login screen */
    if (login_is_locked()) {
        gui_event_t ev;
        while (input_poll(&ev)) {
            login_handle_event(&ev);
        }

        /* If event unlocked the screen, force a full refresh on next tick */
        if (!login_is_locked()) {
            wallpaper_done = false;
            fb_flush_all();
            return;
        }

        uint32_t *bb = fb_get_backbuffer();
        if (!bb) return;
        int stride = fb_get_stride();

        if (!wallpaper_done) draw_wallpaper();
        blit_wallpaper(bb, stride);

        login_render(bb, stride, scr_w, scr_h);
        cursor_draw();

        fb_add_damage(0, 0, scr_w, scr_h);
        static int login_frame = 0;
        if (login_frame++ < 3) {
            fb_flush_all();
        } else {
            fb_flush();
        }
        return;
    }

    gui_event_t ev;
    while (input_poll(&ev)) {
        if (ev.type == EV_MOUSE_MOVE) {
            desktop_hover(ev.x, ev.y);
            wm_handle_event(&ev);
            continue;
        }
        if (ev.type == EV_MOUSE_DOWN) {
            if (desktop_click(ev.x, ev.y)) continue;
        }
        if (ev.type == EV_KEY_DOWN) {
            /* Global shortcuts: Super/Win key, Super+Space, or F1 toggles Start Menu */
            if (ev.keycode == KEY_LEFT_META || ev.keycode == KEY_RIGHT_META ||
                ((ev.modifiers & KMOD_META) && ev.keycode == KEY_SPACE) ||
                ev.keycode == KEY_F1) {
                menu_open = !menu_open;
                if (menu_open && menu_hot < 0) menu_hot = 0;
                continue;
            }
            if (menu_open) {
                if (ev.keycode == KEY_DOWN) {
                    menu_hot = (menu_hot + 1) % APP_COUNT;
                    continue;
                }
                if (ev.keycode == KEY_UP) {
                    menu_hot = (menu_hot - 1 + APP_COUNT) % APP_COUNT;
                    continue;
                }
                if (ev.keycode == KEY_ENTER) {
                    if (menu_hot >= 0 && menu_hot < APP_COUNT) {
                        const char *aname = apps[menu_hot].name;
                        menu_open = false;
                        desktop_launch(aname);
                    }
                    continue;
                }
                if (ev.keycode == KEY_ESCAPE) {
                    menu_open = false;
                    continue;
                }
            }
            if ((ev.modifiers & KMOD_META) && ev.keycode == KEY_D) {
                wm_minimize_all();
                continue;
            }
            if ((ev.modifiers & KMOD_ALT) && ev.keycode == KEY_TAB) {
                if (ev.modifiers & KMOD_SHIFT) wm_cycle_prev();
                else wm_cycle_next();
                continue;
            }
            if ((ev.modifiers & KMOD_ALT) && ev.keycode == KEY_F4) {
                wm_window_t *f = wm_focused();
                if (f) wm_destroy(f);
                continue;
            }
        }
        if (ev.type == EV_KEY_UP) {
            if (ev.keycode == KEY_LEFT_ALT || ev.keycode == KEY_RIGHT_ALT) {
                wm_alt_tab_end();
            }
        }
        power_state_t pwr_check;
        power_get_state(&pwr_check);
        if (pwr_check.is_standby) {
            power_set_standby(false);
            fb_add_damage(0, 0, scr_w, scr_h);
        }
        wm_handle_event(&ev);
    }

    speaker_poll();

    if (term_service_ptr) term_service_ptr();

    /* The clock only needs redrawing once a second */
    uint32_t secs = timer_get_seconds();
    if (secs != last_clock_s) {
        last_clock_s = secs;
        desktop_format_clock(clock_buf, sizeof(clock_buf), true);
        fb_add_damage(scr_w - 240, 0, 240, topbar_h);
    }

    uint32_t *bb = fb_get_backbuffer();
    if (!bb) return;
    int stride = fb_get_stride();

    cursor_erase();

    if (!wallpaper_done) draw_wallpaper();
    blit_wallpaper(bb, stride);

    wm_compose();

    /* Render Rust GUI windows on top of C windows */
    rust_gui_render_frame();

    draw_desktop_icons(bb, stride);
    draw_top_bar(bb, stride);
    draw_left_nav(bb, stride);
    draw_dock(bb, stride);
    draw_menu(bb, stride);

    /* Track component hover damages */
    static int prev_dock_hot = -1;
    static int prev_lnav_hot = -1;
    static int prev_menu_hot = -1;
    static bool prev_menu_open = false;

    if (dock_hot != prev_dock_hot) {
        fb_add_damage(dock_x - 10, dock_y - 10, dock_w + 20, dock_h + 20);
        prev_dock_hot = dock_hot;
    }
    if (lnav_hot != prev_lnav_hot) {
        fb_add_damage(lnav_x - 10, lnav_y - 10, lnav_w + 20, lnav_h + 20);
        prev_lnav_hot = lnav_hot;
    }
    if (menu_hot != prev_menu_hot || menu_open != prev_menu_open) {
        fb_add_damage(8, scr_h - 350 - 64, 280, 350);
        prev_menu_hot = menu_hot;
        prev_menu_open = menu_open;
    }

    if (wm_capture_active()) {
        fb_add_damage(0, 0, scr_w, scr_h);
    }

    /* Standby Mode display veil */
    power_state_t pwr_draw;
    power_get_state(&pwr_draw);
    if (pwr_draw.is_standby) {
        rect_t full = { 0, 0, scr_w, scr_h };
        draw_rect_aa(bb, stride, &full, RGB(0, 0, 0), 220);
        rect_t card = { (scr_w - 380) / 2, (scr_h - 160) / 2, 380, 160 };
        draw_round_rect(bb, stride, &card, 16, RGB(16, 22, 34), 245);
        draw_round_outline(bb, stride, &card, 16, RGB(70, 110, 175), 2, 255);
        draw_text_bb(bb, stride, font_bold(), card.x + 40, card.y + 26, "STANDBY MODE (LOW POWER)", RGB(120, 190, 255));
        draw_text_bb(bb, stride, font_ui(), card.x + 36, card.y + 60, "System suspended in energy-saving sleep state.", RGB(200, 220, 245));
        draw_text_bb(bb, stride, font_bold(), card.x + 42, card.y + 96, "Press any key or move mouse to resume", RGB(80, 225, 150));
        fb_add_damage(card.x - 4, card.y - 4, card.w + 8, card.h + 8);
    }

    cursor_draw();

    static int frame_no = 0;
    if (frame_no++ < 5) {
        fb_flush_all();
    } else {
        fb_flush();
    }
}

void desktop_run(void) {
    uint32_t last_frame_ticks = timer_get_ticks();
    for (;;) {
        uint32_t now = timer_get_ticks();
        if (now - last_frame_ticks >= 16) {
            last_frame_ticks = now;
            desktop_tick();
        } else {
            input_pump();
        }
        process_yield();
    }
}
