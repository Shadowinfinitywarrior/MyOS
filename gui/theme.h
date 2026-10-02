#ifndef GUI_THEME_H
#define GUI_THEME_H

#include "blit.h"

/* One palette for the whole desktop, so the compositor, the window manager and
 * the apps cannot drift apart. Colours are 0x00RRGGBB. */

#define TH_BG_DEEP        RGB(0x0B, 0x0F, 0x1A)   /* wallpaper top    */
#define TH_BG_MID         RGB(0x14, 0x1D, 0x33)   /* wallpaper mid    */
#define TH_BG_LOW         RGB(0x1E, 0x2A, 0x47)   /* wallpaper bottom */

#define TH_ACCENT         RGB(0x4C, 0x8D, 0xF6)
#define TH_ACCENT_DEEP    RGB(0x2F, 0x6B, 0xCE)
#define TH_ACCENT_SOFT    RGB(0x1D, 0x33, 0x57)

#define TH_WIN_BG         RGB(0x1A, 0x20, 0x2C)
#define TH_WIN_BG_FOCUS   RGB(0x1E, 0x25, 0x33)
#define TH_TITLE          RGB(0x24, 0x2D, 0x3F)
#define TH_TITLE_FOCUS    RGB(0x2C, 0x3A, 0x54)
#define TH_TITLE_TEXT     RGB(0xE6, 0xEC, 0xF5)
#define TH_TITLE_TEXT_DIM RGB(0x8A, 0x97, 0xAD)
#define TH_BORDER         RGB(0x33, 0x3E, 0x52)
#define TH_BORDER_FOCUS   RGB(0x4A, 0x6B, 0xA0)

#define TH_TEXT           RGB(0xD6, 0xDE, 0xEA)
#define TH_TEXT_DIM       RGB(0x8A, 0x97, 0xAD)
#define TH_TEXT_BRIGHT    RGB(0xFF, 0xFF, 0xFF)

#define TH_TASKBAR        RGB(0x12, 0x18, 0x24)
#define TH_TASKBAR_EDGE   RGB(0x24, 0x2E, 0x40)

#define TH_TERM_BG        RGB(0x0A, 0x0D, 0x14)
#define TH_TERM_FG        RGB(0xC8, 0xD4, 0xE4)
#define TH_TERM_CURSOR    RGB(0x4C, 0x8D, 0xF6)
#define TH_TERM_PROMPT    RGB(0x4C, 0x8D, 0xF6)
#define TH_TERM_OK        RGB(0x5A, 0xD1, 0x8A)
#define TH_TERM_WARN      RGB(0xE8, 0xC5, 0x6A)
#define TH_TERM_ERR       RGB(0xF2, 0x6D, 0x78)

#define TH_BTN_HOVER      RGB(0x33, 0x44, 0x5E)
#define TH_BTN_CLOSE      RGB(0xE0, 0x4F, 0x5F)
#define TH_BTN_MIN        RGB(0xE8, 0xB0, 0x4A)
#define TH_BTN_MAX        RGB(0x5A, 0xC8, 0x7A)

#define TH_SHADOW         RGB(0x00, 0x00, 0x00)

/* The 16 ANSI colours the vty understands, as 0x00RRGGBB, plus a foreground
 * derived from the bright/regular flag. */
extern const color_t vga_to_rgb[16];
static inline color_t vga_color_rgb(int idx, bool bright) {
    return vga_to_rgb[(idx & 7) + (bright ? 8 : 0)];
}

#endif
