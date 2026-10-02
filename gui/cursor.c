#include "cursor.h"
#include "theme.h"
#include "../drivers/framebuffer.h"

/* Cursor bitmaps: 1 = ink, 2 = outline/shadow, 0 = transparent. Keeping them
 * as small string art makes the shapes readable and editable. */
static const char *arrow_shape[CURSOR_H] = {
    "X...........",
    "XX..........",
    "X.X.........",
    "X..X........",
    "X...X.......",
    "X....X......",
    "X.....X.....",
    "X......X....",
    "X.......X...",
    "X........X..",
    "X.....XXXXX.",
    "X..X..X.....",
    "X.X.X..X....",
    "XX..X..X....",
    "X...X..X....",
    "....X..X....",
    "...X...X....",
    "..X....X....",
    ".X..........",
};

static const char *hand_shape[CURSOR_H] = {
    "....XX......",
    "...X..X..X..",
    "...X..X..X..",
    "...X..X..X..",
    "...X..X..X..",
    "...XXXXXXX..",
    "..X..X..XX..",
    "..X..X..X.X.",
    "..XXXXXX.XX.",
    "...X..X..XX.",
    "...X..X..X..",
    "...X..X..X..",
    "...X..X..X..",
    "...X..X..X..",
    "..XX..XX.X..",
    "..X.....X...",
    "..X.....X...",
    ".XX.....XX..",
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

void cursor_init(void) {
    shape = CUR_ARROW;
    hot_x = 0;
    hot_y = 0;
    last_x = last_y = -1;
}

void cursor_set_shape(cursor_shape_t s) { shape = s; }
cursor_shape_t cursor_shape(void) { return shape; }
void cursor_set_hotspot(int x, int y) { hot_x = x; hot_y = y; }

static inline void put(int x, int y, color_t c, uint32_t *bb, int stride) {
    fb_info_t *info = fb_get_info();
    if (x < 0 || y < 0 || x >= (int)info->width || y >= (int)info->height) return;
    bb[(size_t)y * (size_t)stride + x] = c;
}

void cursor_erase(void) {
    last_x = last_y = -1;
}

void cursor_draw(void) {
    uint32_t *bb = fb_get_backbuffer();
    if (!bb) return;
    int stride = fb_get_stride();
    int x = input_mouse_x() - hot_x;
    int y = input_mouse_y() - hot_y;
    /* No early-out on an unchanged position: the desktop repaints the whole
     * work area every frame, so a cursor drawn only on frames where the mouse
     * moved is erased by the very next repaint and flickers out of existence.
     * 12x19 pixels per frame is cheaper than getting this wrong. */
    last_x = x;
    last_y = y;

    const char **map = shape_for(shape);

    /* Shadow first, offset down-right, so the arrow reads on any background. */
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            if (map[row][col] != 'X') continue;
            put(x + col + 2, y + row + 2, RGB(0x00, 0x00, 0x00), bb, stride);
        }
    }
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            if (map[row][col] != 'X') continue;
            put(x + col, y + row, RGB(0xFF, 0xFF, 0xFF), bb, stride);
        }
    }
    /* The area under the old and new positions needs repainting. */
    fb_add_damage(x - 3, y - 3, CURSOR_W + 6, CURSOR_H + 6);
}
