#ifndef GUI_THEME_H
#define GUI_THEME_H

#include "blit.h"

/* Modern desktop theme palette (inspired by modern dark acrylic / macOS / Tokyo Night) */

#define TH_BG_DEEP        RGB(0x0A, 0x0E, 0x17)   /* wallpaper base deep obsidian */
#define TH_BG_MID         RGB(0x13, 0x1A, 0x2A)   /* wallpaper mid cosmic sapphire */
#define TH_BG_LOW         RGB(0x1A, 0x24, 0x3B)   /* wallpaper lower nebula */

#define TH_ACCENT         RGB(0x38, 0x8B, 0xFD)   /* vibrant modern electric azure */
#define TH_ACCENT_DEEP    RGB(0x1F, 0x6F, 0xEB)
#define TH_ACCENT_SOFT    RGB(0x1F, 0x29, 0x3D)   /* frosted glass pill bg */

#define TH_WIN_BG         RGB(0x16, 0x1B, 0x22)   /* window client dark background */
#define TH_WIN_BG_FOCUS   RGB(0x16, 0x1B, 0x22)
#define TH_TITLE          RGB(0x1F, 0x24, 0x2C)   /* acrylic titlebar unfocused */
#define TH_TITLE_FOCUS    RGB(0x21, 0x26, 0x30)   /* acrylic titlebar focused */
#define TH_TITLE_TEXT     RGB(0xF0, 0xF6, 0xFC)   /* crisp snow white */
#define TH_TITLE_TEXT_DIM RGB(0x8B, 0x94, 0x9E)
#define TH_BORDER         RGB(0x30, 0x36, 0x3D)   /* 1px subtle dark border */
#define TH_BORDER_FOCUS   RGB(0x48, 0x52, 0x63)   /* 1px focused border */

#define TH_TEXT           RGB(0xE6, 0xED, 0xF3)
#define TH_TEXT_DIM       RGB(0x8B, 0x94, 0x9E)
#define TH_TEXT_BRIGHT    RGB(0xFF, 0xFF, 0xFF)

#define TH_TASKBAR        RGB(0x0D, 0x11, 0x17)   /* frosted dark bottom taskbar */
#define TH_TASKBAR_EDGE   RGB(0x21, 0x26, 0x2D)   /* 1px glass highlight top */

#define TH_TERM_BG        RGB(0x0D, 0x11, 0x17)   /* modern terminal deep slate */
#define TH_TERM_FG        RGB(0xE6, 0xED, 0xF3)
#define TH_TERM_CURSOR    RGB(0x58, 0xA6, 0xFF)
#define TH_TERM_PROMPT    RGB(0x58, 0xA6, 0xFF)
#define TH_TERM_OK        RGB(0x3F, 0xB9, 0x50)
#define TH_TERM_WARN      RGB(0xD2, 0x99, 0x22)
#define TH_TERM_ERR       RGB(0xF8, 0x51, 0x49)

#define TH_BTN_HOVER      RGB(0x30, 0x36, 0x3D)
#define TH_BTN_CLOSE      RGB(0xFF, 0x5F, 0x56)   /* modern traffic light coral red */
#define TH_BTN_MIN        RGB(0xFF, 0xBD, 0x2E)   /* modern traffic light amber */
#define TH_BTN_MAX        RGB(0x27, 0xC9, 0x3F)   /* modern traffic light emerald */

#define TH_SHADOW         RGB(0x00, 0x00, 0x00)

/* The 16 ANSI colours the vty understands, as 0x00RRGGBB, plus a foreground
 * derived from the bright/regular flag. */
extern const color_t vga_to_rgb[16];
static inline color_t vga_color_rgb(int idx, bool bright) {
    return vga_to_rgb[(idx & 7) + (bright ? 8 : 0)];
}

#endif
