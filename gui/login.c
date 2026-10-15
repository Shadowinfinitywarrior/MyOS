#include "login.h"
#include "glass.h"
#include "logo.h"
#include "theme.h"
#include "text.h"
#include "cursor.h"
#include "wm.h"
#include "../lib/string.h"
#include "../kernel/heap.h"
#include "../kernel/timer.h"
#include "../kernel/storage.h"
#include "../drivers/framebuffer.h"

static auth_user_t users[AUTH_MAX_USERS];
static int user_count = 0;
static char current_user[AUTH_NAME_MAX] = "myos";
static bool system_locked = true;
static bool in_storage_load = false;

/* Login screen UI state */
static char input_user[AUTH_NAME_MAX] = "";
static char input_pass[AUTH_PASS_MAX] = "";
static int focused_field = 0; /* 0 = username, 1 = password, 2 = sign in button */
static char error_message[64] = "";
static uint32_t error_time = 0;
static bool button_hover = false;

/* Initialize users with default credentials */
void auth_init(void) {
    user_count = 0;
    /* Default user: username = "myos", password = "myos" */
    strncpy(users[0].username, "myos", AUTH_NAME_MAX - 1);
    users[0].username[AUTH_NAME_MAX - 1] = '\0';
    strncpy(users[0].password, "myos", AUTH_PASS_MAX - 1);
    users[0].password[AUTH_PASS_MAX - 1] = '\0';
    users[0].active = true;
    user_count = 1;

    strncpy(current_user, "myos", AUTH_NAME_MAX - 1);
    system_locked = true;
    focused_field = 0;
    input_user[0] = '\0';
    input_pass[0] = '\0';
    error_message[0] = '\0';

    /* Restore accounts from persistent storage */
    in_storage_load = true;
    storage_load_users();
    in_storage_load = false;
}

bool auth_validate(const char *username, const char *password) {
    if (!username || !password) return false;
    for (int i = 0; i < user_count; i++) {
        if (users[i].active &&
            strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            return true;
        }
    }
    return false;
}

bool auth_add_user(const char *username, const char *password) {
    if (!username || !password || username[0] == '\0') return false;
    /* Check if user already exists -> update password */
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            strncpy(users[i].password, password, AUTH_PASS_MAX - 1);
            users[i].password[AUTH_PASS_MAX - 1] = '\0';
            users[i].active = true;
            if (!in_storage_load) storage_save_users();
            return true;
        }
    }
    /* Add new user */
    if (user_count >= AUTH_MAX_USERS) return false;
    strncpy(users[user_count].username, username, AUTH_NAME_MAX - 1);
    users[user_count].username[AUTH_NAME_MAX - 1] = '\0';
    strncpy(users[user_count].password, password, AUTH_PASS_MAX - 1);
    users[user_count].password[AUTH_PASS_MAX - 1] = '\0';
    users[user_count].active = true;
    user_count++;
    if (!in_storage_load) storage_save_users();
    return true;
}

bool auth_change_password(const char *username, const char *new_password) {
    if (!username || !new_password) return false;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            strncpy(users[i].password, new_password, AUTH_PASS_MAX - 1);
            users[i].password[AUTH_PASS_MAX - 1] = '\0';
            if (!in_storage_load) storage_save_users();
            return true;
        }
    }
    return false;
}

int auth_export_records(auth_user_t *dest, int max) {
    if (!dest || max <= 0) return 0;
    int cnt = 0;
    for (int i = 0; i < user_count && cnt < max; i++) {
        if (users[i].active) {
            dest[cnt] = users[i];
            cnt++;
        }
    }
    return cnt;
}

void auth_import_record(const auth_user_t *rec) {
    if (!rec || rec->username[0] == '\0') return;
    auth_add_user(rec->username, rec->password);
}

int auth_get_users(char names[][AUTH_NAME_MAX], int max_users) {
    int count = 0;
    for (int i = 0; i < user_count && count < max_users; i++) {
        if (users[i].active) {
            strncpy(names[count], users[i].username, AUTH_NAME_MAX - 1);
            names[count][AUTH_NAME_MAX - 1] = '\0';
            count++;
        }
    }
    return count;
}

const char *auth_get_current_user(void) {
    return current_user;
}

void auth_set_current_user(const char *username) {
    if (!username) return;
    strncpy(current_user, username, AUTH_NAME_MAX - 1);
    current_user[AUTH_NAME_MAX - 1] = '\0';
}

bool login_is_locked(void) {
    return system_locked;
}

void login_lock(void) {
    system_locked = true;
    input_user[0] = '\0';
    input_pass[0] = '\0';
    error_message[0] = '\0';
    focused_field = 0;
}

void login_unlock(void) {
    system_locked = false;
    input_pass[0] = '\0';
    error_message[0] = '\0';
    wm_window_t *term = wm_find("MyOS Terminal");
    if (!term) term = wm_find("Terminal");
    if (term) wm_focus(term);
}

/* Helper functions for direct drawing onto the backbuffer */
static inline void put_pixel(uint32_t *bb, int stride, int x, int y, int scr_w, int scr_h, color_t c) {
    if (x >= 0 && y >= 0 && x < scr_w && y < scr_h) {
        bb[(size_t)y * (size_t)stride + x] = c;
    }
}

static void draw_bb_text(uint32_t *bb, int stride, const baked_font_t *f, int x, int y,
                         int scr_w, int scr_h, const char *str, color_t c) {
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
                if (bits[r * pitch_bits + (col / 8)] & (1 << (col % 8)))
                    bb[(size_t)py * (size_t)stride + px_x] = c;
            }
        }
        pen += xa;
    }
}

static void draw_bb_round_rect(uint32_t *bb, int stride, int scr_w, int scr_h,
                               const rect_t *r, int radius, color_t c, int alpha) {
    if (r->w <= 0 || r->h <= 0) return;
    if (radius <= 0) {
        for (int y = r->y; y < r->y + r->h; y++) {
            if (y < 0 || y >= scr_h) continue;
            for (int x = r->x; x < r->x + r->w; x++) {
                if (x < 0 || x >= scr_w) continue;
                if (alpha >= 255) bb[(size_t)y * (size_t)stride + x] = c;
                else {
                    uint32_t d = bb[(size_t)y * (size_t)stride + x];
                    uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
                    uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
                    uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
                    bb[(size_t)y * (size_t)stride + x] = (r0 << 16) | (g0 << 8) | b0;
                }
            }
        }
        return;
    }

    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;

    rect_t body = { r->x, r->y + radius, r->w, r->h - radius * 2 };
    rect_t top  = { r->x + radius, r->y, r->w - radius * 2, radius };
    rect_t bot  = { r->x + radius, r->y + r->h - radius, r->w - radius * 2, radius };

    /* Fill straight rectangles */
    const rect_t rects[3] = { body, top, bot };
    for (int ri = 0; ri < 3; ri++) {
        const rect_t *rc = &rects[ri];
        for (int y = rc->y; y < rc->y + rc->h; y++) {
            if (y < 0 || y >= scr_h) continue;
            for (int x = rc->x; x < rc->x + rc->w; x++) {
                if (x < 0 || x >= scr_w) continue;
                if (alpha >= 255) bb[(size_t)y * (size_t)stride + x] = c;
                else {
                    uint32_t d = bb[(size_t)y * (size_t)stride + x];
                    uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
                    uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
                    uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
                    bb[(size_t)y * (size_t)stride + x] = (r0 << 16) | (g0 << 8) | b0;
                }
            }
        }
    }

    /* Subpixel antialiased corner circles */
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
                    put_pixel(bb, stride, x, y, scr_w, scr_h, c);
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

static void draw_bb_round_outline(uint32_t *bb, int stride, int scr_w, int scr_h,
                                  const rect_t *r, int radius, color_t c, int alpha) {
    if (r->w <= 0 || r->h <= 0) return;
    if (radius * 2 > r->w) radius = r->w / 2;
    if (radius * 2 > r->h) radius = r->h / 2;
    rect_t top = { r->x + radius, r->y, r->w - radius * 2, 1 };
    rect_t bot = { r->x + radius, r->y + r->h - 1, r->w - radius * 2, 1 };
    rect_t lft = { r->x, r->y + radius, 1, r->h - radius * 2 };
    rect_t rgt = { r->x + r->w - 1, r->y + radius, 1, r->h - radius * 2 };
    draw_bb_round_rect(bb, stride, scr_w, scr_h, &top, 0, c, alpha);
    draw_bb_round_rect(bb, stride, scr_w, scr_h, &bot, 0, c, alpha);
    draw_bb_round_rect(bb, stride, scr_w, scr_h, &lft, 0, c, alpha);
    draw_bb_round_rect(bb, stride, scr_w, scr_h, &rgt, 0, c, alpha);

    int ro = radius * radius;
    int ri = (radius - 1) * (radius - 1);
    for (int q = 0; q < 4; q++) {
        int sx = (q & 1) ? 1 : -1, sy = (q & 2) ? 1 : -1;
        int cx = (q & 1) ? (r->x + r->w - radius - 1) : (r->x + radius);
        int cy = (q & 2) ? (r->y + r->h - radius - 1) : (r->y + radius);
        for (int dy = 0; dy <= radius; dy++) {
            for (int dx = 0; dx <= radius; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 <= ro && d2 >= ri) {
                    int px_x = cx + sx * dx;
                    int px_y = cy + sy * dy;
                    if (px_x >= 0 && px_y >= 0 && px_x < scr_w && px_y < scr_h) {
                        uint32_t d = bb[(size_t)px_y * (size_t)stride + px_x];
                        uint32_t r0 = ((d >> 16) & 0xFF) + ((((c >> 16) & 0xFF) - ((d >> 16) & 0xFF)) * alpha) / 255;
                        uint32_t g0 = ((d >> 8) & 0xFF) + ((((c >> 8) & 0xFF) - ((d >> 8) & 0xFF)) * alpha) / 255;
                        uint32_t b0 = (d & 0xFF) + (((c & 0xFF) - (d & 0xFF)) * alpha) / 255;
                        bb[(size_t)px_y * (size_t)stride + px_x] = (r0 << 16) | (g0 << 8) | b0;
                    }
                }
            }
        }
    }
}

/* ---- cached static backdrop -----------------------------------------------
 * The frosted tint is a full-screen alpha blend over ~786k pixels, and the
 * card, shadow, logo and titles add more blending on top. None of that changes
 * between frames, so the composited result is cached in login_bg and only
 * rebuilt when the wallpaper changes. Each repaint is then one straight copy
 * plus the small dynamic layers (input boxes, text, caret, button). */
static uint32_t *login_bg;
static int       login_bg_w, login_bg_h;
static uint32_t  login_bg_sig;
static bool      login_bg_valid;

/* Sample a spread of wallpaper pixels so a theme/wallpaper change invalidates
 * the cache without the desktop having to tell us explicitly. */
static uint32_t login_wallpaper_sig(uint32_t *bb, int stride, int w, int h) {
    uint32_t s = 0x811C9DC5u;
    int mw = w > 1 ? w - 1 : 1;
    int mh = h > 1 ? h - 1 : 1;
    for (int i = 0; i < 64; i++) {
        int x = (int)((uint64_t)i * (uint64_t)mw / 63);
        int y = (int)((((uint32_t)i * 2654435761u) >> 26) * (uint32_t)mh / 63);
        s = (s ^ bb[(size_t)y * (size_t)stride + x]) * 16777619u;
    }
    return s ? s : 1u;
}

/* Draw the wallpaper-independent static layers (tint, card, shadow, glint,
 * logo, titles) onto dst. */
static void login_draw_static(uint32_t *dst, int dstride, int w, int h) {
    rect_t screen = { 0, 0, w, h };
    draw_bb_round_rect(dst, dstride, w, h, &screen, 0, RGB(0x06, 0x09, 0x12), 170);

    int card_w = 370, card_h = 350;
    int card_x = (w - card_w) / 2;
    int card_y = (h - card_h) / 2 - 20;
    rect_t card = { card_x, card_y, card_w, card_h };
    glass_render_panel(dst, dstride, &card);


    int logo_cx = card_x + card_w / 2;
    int logo_cy = card_y + 50;
    logo_draw_buffer(dst, dstride, w, h, logo_cx, logo_cy, 58);

    const char *title = "Welcome to MyOS";
    int tw = text_width(font_bold(), title);
    draw_bb_text(dst, dstride, font_bold(), card_x + (card_w - tw) / 2, card_y + 86,
                 w, h, title, RGB(0xF2, 0xF8, 0xFF));

    const char *sub = "GLASSMORPHIC DESKTOP • SYSTEM READY";
    int sw = text_width(font_ui(), sub);
    draw_bb_text(dst, dstride, font_ui(), card_x + (card_w - sw) / 2, card_y + 108,
                 w, h, sub, RGB(0x9E, 0xC4, 0xFA));

    const char *sub2 = "Sign in to access your glass session";
    int s2w = text_width(font_ui(), sub2);
    draw_bb_text(dst, dstride, font_ui(), card_x + (card_w - s2w) / 2, card_y + 126,
                 w, h, sub2, RGB(0x76, 0x8E, 0xB0));
}

/* Render login screen */
void login_render(uint32_t *bb, int stride, int scr_w, int scr_h) {
    if (!system_locked) return;

    /* Keep the cache matched to the current screen size. */
    if (login_bg && (login_bg_w != scr_w || login_bg_h != scr_h)) {
        kfree(login_bg);
        login_bg = NULL;
        login_bg_valid = false;
    }
    if (!login_bg) {
        login_bg = (uint32_t *)kmalloc((size_t)scr_w * (size_t)scr_h * 4u);
        if (login_bg) { login_bg_w = scr_w; login_bg_h = scr_h; login_bg_valid = false; }
    }

    bool full = false;
    if (login_bg) {
        uint32_t sig = login_wallpaper_sig(bb, stride, scr_w, scr_h);
        if (!login_bg_valid || sig != login_bg_sig) {
            for (int y = 0; y < scr_h; y++)
                memcpy(login_bg + (size_t)y * (size_t)scr_w,
                       (uint8_t *)bb + (size_t)y * (size_t)stride * 4u,
                       (size_t)scr_w * 4u);
            login_draw_static(login_bg, scr_w, scr_w, scr_h);
            login_bg_sig = sig;
            login_bg_valid = true;
            full = true;
        }
        for (int y = 0; y < scr_h; y++)
            memcpy((uint8_t *)bb + (size_t)y * (size_t)stride * 4u,
                   login_bg + (size_t)y * (size_t)scr_w,
                   (size_t)scr_w * 4u);
    } else {
        /* No cache available (allocation failed): render static layers inline. */
        login_draw_static(bb, stride, scr_w, scr_h);
        full = true;
    }

    /* ---- dynamic layers: small, redrawn every frame ---- */
    int card_w = 370, card_h = 350;
    int card_x = (scr_w - card_w) / 2;
    int card_y = (scr_h - card_h) / 2 - 20;

    int inp_w = 290, inp_h = 36;
    int inp_x = card_x + (card_w - inp_w) / 2;

    /* Username Input Box: frosted glass using primitives */
    int u_y = card_y + 155;
    rect_t u_box = { inp_x, u_y, inp_w, inp_h };
    bool u_focus = (focused_field == 0);
    glass_render_surface(bb, stride, &u_box, MyOSTheme.radius_input,
                         MyOSTheme.glass_surface_primary, MyOSTheme.alpha_card,
                         false, false, true);
    if (u_focus) {
        draw_bb_round_outline(bb, stride, scr_w, scr_h, &u_box, 8,
                              MyOSTheme.accent_primary, 220);
        rect_t u_shine = { inp_x + 6, u_y + 1, inp_w - 12, 1 };
        draw_bb_round_rect(bb, stride, scr_w, scr_h, &u_shine, 0,
                           MyOSTheme.highlight_shine, MyOSTheme.alpha_shine);
    }

    /* Username text or placeholder */
    if (input_user[0] == '\0') {
        draw_bb_text(bb, stride, font_ui(), inp_x + 14, u_y + 10, scr_w, scr_h,
                     "Username", RGB(0x60, 0x74, 0x94));
    } else {
        draw_bb_text(bb, stride, font_ui(), inp_x + 14, u_y + 10, scr_w, scr_h,
                     input_user, RGB(0xFF, 0xFF, 0xFF));
    }
    if (u_focus && ((timer_get_ms() / 500) % 2 == 0)) {
        int cx = inp_x + 14 + text_width(font_ui(), input_user);
        rect_t cur = { cx, u_y + 8, 2, 20 };
        draw_bb_round_rect(bb, stride, scr_w, scr_h, &cur, 0, RGB(0x58, 0xB5, 0xFF), 255);
    }

    /* Password Input Box: frosted glass using primitives */
    int p_y = u_y + 46;
    rect_t p_box = { inp_x, p_y, inp_w, inp_h };
    bool p_focus = (focused_field == 1);
    glass_render_surface(bb, stride, &p_box, MyOSTheme.radius_input,
                         MyOSTheme.glass_surface_primary, MyOSTheme.alpha_card,
                         false, false, true);
    if (p_focus) {
        draw_bb_round_outline(bb, stride, scr_w, scr_h, &p_box, 8,
                              MyOSTheme.accent_primary, 220);
        rect_t p_shine = { inp_x + 6, p_y + 1, inp_w - 12, 1 };
        draw_bb_round_rect(bb, stride, scr_w, scr_h, &p_shine, 0,
                           MyOSTheme.highlight_shine, MyOSTheme.alpha_shine);
    }
    if (input_pass[0] == '\0') {
        draw_bb_text(bb, stride, font_ui(), inp_x + 14, p_y + 10, scr_w, scr_h,
                     "Password", RGB(0x60, 0x74, 0x94));
    } else {
        /* Draw bullet dots */
        int pass_len = (int)strlen(input_pass);
        int dot_x = inp_x + 14;
        for (int d = 0; d < pass_len && d < 24; d++) {
            rect_t dot = { dot_x, p_y + 15, 6, 6 };
            draw_bb_round_rect(bb, stride, scr_w, scr_h, &dot, 3, RGB(0xFF, 0xFF, 0xFF), 255);
            dot_x += 10;
        }
    }
    if (p_focus && ((timer_get_ms() / 500) % 2 == 0)) {
        int cx = inp_x + 14 + ((int)strlen(input_pass) * 10);
        rect_t cur = { cx, p_y + 8, 2, 20 };
        draw_bb_round_rect(bb, stride, scr_w, scr_h, &cur, 0, RGB(0x58, 0xB5, 0xFF), 255);
    }

    /* Error Message display (if any) */
    if (error_message[0]) {
        int ew = text_width(font_ui(), error_message);
        draw_bb_text(bb, stride, font_ui(), card_x + (card_w - ew) / 2, p_y + 40,
                     scr_w, scr_h, error_message, RGB(0xFF, 0x5F, 0x56));
    }

    /* Sign In Button: electric azure glass pill using primitives */
    int btn_y = card_y + card_h - 56;
    rect_t btn = { inp_x, btn_y, inp_w, 38 };
    glass_render_button(bb, stride, &btn, "Sign In", font_bold(), button_hover, false, RGB(0xFF, 0xFF, 0xFF));

    /* Damage only what can change frame to frame: the full screen when the
     * cached backdrop was just rebuilt, otherwise just the card interior that
     * holds the input boxes, error text, button and caret. */
    if (full) {
        fb_add_damage(0, 0, scr_w, scr_h);
    } else {
        int dy = u_y - 8;
        int dh = (card_y + card_h - 12) - dy;
        if (dy < 0) { dh += dy; dy = 0; }
        if (dh > 0) fb_add_damage(card_x, dy, card_w, dh);
    }
}

static void try_login(void) {
    if (input_user[0] == '\0') {
        strncpy(error_message, "Please enter username", sizeof(error_message) - 1);
        focused_field = 0;
        return;
    }
    if (auth_validate(input_user, input_pass)) {
        auth_set_current_user(input_user);
        login_unlock();
    } else {
        strncpy(error_message, "Invalid username or password", sizeof(error_message) - 1);
        input_pass[0] = '\0';
        focused_field = 1;
        error_time = timer_get_seconds();
    }
}

/* Event handler for login screen */
bool login_handle_event(const gui_event_t *ev) {
    if (!system_locked || !ev) return false;

    fb_info_t *fb = fb_get_info();
    int scr_w = fb ? (int)fb->width : 1024;
    int scr_h = fb ? (int)fb->height : 768;

    int card_w = 370, card_h = 350;
    int card_x = (scr_w - card_w) / 2;
    int card_y = (scr_h - card_h) / 2 - 20;

    int inp_w = 290, inp_h = 36;
    int inp_x = card_x + (card_w - inp_w) / 2;
    int u_y = card_y + 155;
    int p_y = u_y + 46;
    int btn_y = card_y + card_h - 56;

    rect_t u_box = { inp_x, u_y, inp_w, inp_h };
    rect_t p_box = { inp_x, p_y, inp_w, inp_h };
    rect_t btn = { inp_x, btn_y, inp_w, 38 };

    if (ev->type == EV_MOUSE_MOVE) {
        button_hover = rect_contains_point(&btn, ev->x, ev->y);
        return true;
    }

    if (ev->type == EV_MOUSE_DOWN) {
        if (rect_contains_point(&u_box, ev->x, ev->y)) {
            focused_field = 0;
            return true;
        }
        if (rect_contains_point(&p_box, ev->x, ev->y)) {
            focused_field = 1;
            return true;
        }
        if (rect_contains_point(&btn, ev->x, ev->y)) {
            try_login();
            return true;
        }
    }

    if (ev->type == EV_KEY_DOWN) {
        error_message[0] = '\0'; /* clear error on keypress */

        if (ev->ascii == '\t' || ev->keycode == GUIKEY_DOWN) {
            focused_field = (focused_field + 1) % 2;
            return true;
        }
        if (ev->keycode == GUIKEY_UP) {
            focused_field = (focused_field + 1) % 2;
            return true;
        }
        if (ev->ascii == '\r' || ev->ascii == '\n') {
            if (focused_field == 0) {
                focused_field = 1;
            } else {
                try_login();
            }
            return true;
        }
        if (ev->ascii == '\b' || ev->keycode == GUIKEY_BACKSPACE) {
            if (focused_field == 0) {
                int len = (int)strlen(input_user);
                if (len > 0) input_user[len - 1] = '\0';
            } else {
                int len = (int)strlen(input_pass);
                if (len > 0) input_pass[len - 1] = '\0';
            }
            return true;
        }

        /* Printable character */
        if (ev->ascii >= 32 && ev->ascii <= 126) {
            if (focused_field == 0) {
                int len = (int)strlen(input_user);
                if (len < AUTH_NAME_MAX - 2) {
                    input_user[len] = (char)ev->ascii;
                    input_user[len + 1] = '\0';
                }
            } else {
                int len = (int)strlen(input_pass);
                if (len < AUTH_PASS_MAX - 2) {
                    input_pass[len] = (char)ev->ascii;
                    input_pass[len + 1] = '\0';
                }
            }
            return true;
        }
    }

    return true;
}
