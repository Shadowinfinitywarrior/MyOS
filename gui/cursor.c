#include "cursor.h"
#include "theme.h"
#include "../drivers/framebuffer.h"

/* Cursor bitmaps: '#' = black outline, '*' = solid white body, '.' = transparent. */
static const char *arrow_shape[CURSOR_H] = {
    "#...........",
    "##..........",
    "#*#.........",
    "#**#........",
    "#***#.......",
    "#****#......",
    "#*****#.....",
    "#******#....",
    "#*******#...",
    "#********#..",
    "#*****####..",
    "#**#**#.....",
    "#*#.#**#....",
    "##..#**#....",
    "#....#**#...",
    ".....#**#...",
    "......##....",
    "............",
    "............",
};

static const char *hand_shape[CURSOR_H] = {
    "....##......",
    "...#**#..#..",
    "...#**#.#*#.",
    "...#**#.#*#.",
    "...#**#.#*#.",
    "...#*******#",
    "..#********#",
    "..#********#",
    "..#********#",
    "...#*******#",
    "...#*******#",
    "...#*******#",
    "...#*******#",
    "...#*******#",
    "..##*******#",
    "..#********#",
    "..#********#",
    ".##********#",
    "............",
};

static const char *ibeam_shape[CURSOR_H] = {
    "....XX..XX..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "...XXXXXX...",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "....X....X..",
    "...XXXXXX...",
    "............",
    "............",
    "............",
};

static const char *resize_h_shape[CURSOR_H] = {
    "............",
    "............",
    "...X....X...",
    "....X..X....",
    ".....XX.....",
    "..X..XX..X..",
    "...XXXXXX...",
    "..X..XX..X..",
    ".....XX.....",
    "....X..X....",
    "...X....X...",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
};

static const char *resize_v_shape[CURSOR_H] = {
    ".......X....",
    ".......X....",
    "......XX....",
    "......XX....",
    ".....XXXXX..",
    "......XX....",
    "......XX....",
    "....XXXXX...",
    "......XX....",
    "......XX....",
    "......XX....",
    "....XXXXX...",
    "......XX....",
    "......XX....",
    ".....XXXXX..",
    "......XX....",
    "......XX....",
    ".......X....",
    ".......X....",
};

static const char *resize_nwse_shape[CURSOR_H] = {
    "......X.X...",
    ".....X...X..",
    "....X....X..",
    "...X.....X..",
    "..X......X..",
    "..X......X..",
    "..X.....X...",
    ".X..X..X....",
    ".X.X..X.....",
    "..X.X..X....",
    "...X.X..X...",
    "...X..X..X..",
    "..X...X..X..",
    "..X....X..X.",
    ".X.....X..X.",
    ".X......X.X.",
    "............",
    "............",
    "............",
};

static const char *resize_nesw_shape[CURSOR_H] = {
    "...X.X......",
    "..X...X.....",
    "..X....X....",
    "..X.....X...",
    "..X......X..",
    "..X......X..",
    "...X.....X..",
    "....X..X..X.",
    ".....X..X.X.",
    "....X..X..X.",
    "...X..X..X..",
    "..X..X..X...",
    "..X..X...X..",
    ".X..X....X..",
    ".X..X.....X.",
    ".X.X......X.",
    "............",
    "............",
    "............",
};

static const char **shape_for(cursor_shape_t s) {
    switch (s) {
        case CUR_HAND:          return (const char **)hand_shape;
        case CUR_IBEAM:         return (const char **)ibeam_shape;
        case CUR_RESIZE_H:      return (const char **)resize_h_shape;
        case CUR_RESIZE_V:      return (const char **)resize_v_shape;
        case CUR_RESIZE_NWSE:   return (const char **)resize_nwse_shape;
        case CUR_RESIZE_NESW:   return (const char **)resize_nesw_shape;
        case CUR_ARROW:
        default:                return (const char **)arrow_shape;
    }
}

static cursor_shape_t shape = CUR_ARROW;
static int hot_x, hot_y;
static int last_x = -1, last_y = -1;

#define UNDER_W (CURSOR_W + 16)
#define UNDER_H (CURSOR_H + 16)
static uint32_t under_cursor[UNDER_H * UNDER_W];
static int under_x = -1, under_y = -1;
static int under_w = 0, under_h = 0;
static bool under_valid = false;

void cursor_init(void) {
    shape = CUR_ARROW;
    hot_x = 0;
    hot_y = 0;
    last_x = last_y = -1;
    under_valid = false;
}

void cursor_set_shape(cursor_shape_t s) { shape = s; }
cursor_shape_t cursor_shape(void) { return shape; }
void cursor_set_hotspot(int x, int y) { hot_x = x; hot_y = y; }

static inline void put(int x, int y, color_t c, uint32_t *bb, int stride) {
    fb_info_t *info = fb_get_info();
    if (x < 0 || y < 0 || x >= (int)info->width || y >= (int)info->height) return;
    bb[(size_t)y * (size_t)stride + x] = c;
}

static inline void put_blend(int x, int y, color_t c, uint32_t alpha, uint32_t *bb, int stride) {
    fb_info_t *info = fb_get_info();
    if (x < 0 || y < 0 || x >= (int)info->width || y >= (int)info->height) return;
    size_t idx = (size_t)y * (size_t)stride + (size_t)x;
    uint32_t dst = bb[idx];
    uint32_t inv = 256 - alpha;
    uint32_t r = (((dst >> 16) & 0xFF) * inv + (((c >> 16) & 0xFF) * alpha)) >> 8;
    uint32_t g = (((dst >> 8) & 0xFF) * inv + (((c >> 8) & 0xFF) * alpha)) >> 8;
    uint32_t b = ((dst & 0xFF) * inv + ((c & 0xFF) * alpha)) >> 8;
    bb[idx] = (r << 16) | (g << 8) | b;
}

void cursor_erase(void) {
    if (!under_valid) return;
    uint32_t *bb = fb_get_backbuffer();
    if (!bb) { under_valid = false; return; }
    int stride = fb_get_stride();
    fb_info_t *info = fb_get_info();
    if (!info) { under_valid = false; return; }

    for (int r = 0; r < under_h; r++) {
        int py = under_y + r;
        if (py < 0 || py >= (int)info->height) continue;
        for (int c = 0; c < under_w; c++) {
            int px = under_x + c;
            if (px < 0 || px >= (int)info->width) continue;
            bb[(size_t)py * (size_t)stride + px] = under_cursor[r * under_w + c];
        }
    }
    fb_add_damage(under_x, under_y, under_w, under_h);
    under_valid = false;
    last_x = last_y = -1;
}

void cursor_draw(void) {
    uint32_t *bb = fb_get_backbuffer();
    if (!bb) return;
    int stride = fb_get_stride();
    fb_info_t *info = fb_get_info();
    if (!info) return;

    int x = input_mouse_x() - hot_x;
    int y = input_mouse_y() - hot_y;

    if (under_valid && x == last_x && y == last_y) {
        return;
    }

    if (under_valid) {
        cursor_erase();
    }

    last_x = x;
    last_y = y;

    int sx = x - 2;
    int sy = y - 2;
    int sw = CURSOR_W + 8;
    int sh = CURSOR_H + 8;
    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sx + sw > (int)info->width) sw = (int)info->width - sx;
    if (sy + sh > (int)info->height) sh = (int)info->height - sy;
    if (sw <= 0 || sh <= 0 || sw > UNDER_W || sh > UNDER_H) return;

    under_x = sx;
    under_y = sy;
    under_w = sw;
    under_h = sh;

    for (int r = 0; r < sh; r++) {
        for (int c = 0; c < sw; c++) {
            under_cursor[r * sw + c] = bb[(size_t)(sy + r) * (size_t)stride + (sx + c)];
        }
    }
    under_valid = true;

    const char **map = shape_for(shape);

    /* Smooth 2-layer ambient drop shadow with soft alpha falloff */
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            char ch = map[row][col];
            if (ch == '#' || ch == '*' || ch == 'X' || ch == '+') {
                /* Outer soft blur */
                put_blend(x + col + 2, y + row + 3, RGB(0x04, 0x06, 0x0A), 50, bb, stride);
                /* Inner soft shadow */
                put_blend(x + col + 1, y + row + 2, RGB(0x06, 0x09, 0x0E), 110, bb, stride);
            }
        }
    }
    /* Draw outline and solid body */
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            char ch = map[row][col];
            if (ch == '#' || ch == 'X') {
                put(x + col, y + row, RGB(0x10, 0x14, 0x1E), bb, stride);
            } else if (ch == '*') {
                put(x + col, y + row, RGB(0xFF, 0xFF, 0xFF), bb, stride);
            } else if (ch == '+') {
                put(x + col, y + row, RGB(0xD8, 0xDE, 0xE9), bb, stride);
            }
        }
    }
    fb_add_damage(sx, sy, sw, sh);
}
