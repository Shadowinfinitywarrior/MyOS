#ifndef GUI_THEME_H
#define GUI_THEME_H

#include "blit.h"

/* =========================================================================
 * MyOS Glassmorphism Design System Tokens
 * A consistent dark-futurist visual language with translucent frosted glass,
 * subtle blue-violet accents, and soft elevation shadows.
 * ========================================================================= */

/* --- Color Palette --- */

#define TH_BG_DEEP        RGB(0x0C, 0x10, 0x1D)   /* deep obsidian canvas */
#define TH_BG_MID         RGB(0x14, 0x16, 0x2A)   /* mid cosmic sapphire */
#define TH_BG_LOW         RGB(0x1A, 0x1D, 0x32)   /* lower luminous nebula */

/* Primary glass surface: #171D2B, Secondary: #222A3B */
#define TH_GLASS_PRIMARY  RGB(0x17, 0x1D, 0x2B)   /* primary frosted glass */
#define TH_GLASS_SECONDARY RGB(0x22, 0x2A, 0x3B)   /* secondary frosted glass */

/* Accents: #8B9DFF primary, #65D9FF secondary */
#define TH_ACCENT_PRIMARY RGB(0x8B, 0x9D, 0xFF)   /* electric azure */
#define TH_ACCENT_SECONDARY RGB(0x65, 0xD9, 0xFF)  /* luminous cyan */

/* Text colors */
#define TH_TEXT_PRIMARY   RGB(0xF4, 0xF6, 0xFF)   /* crisp snow primary */
#define TH_TEXT_SECONDARY RGB(0xA8, 0xB0, 0xC5)   /* muted secondary */
#define TH_TEXT_DIM       RGB(0x6E, 0x7C, 0x98)   /* dim readable */

/* Glass border: white at 10-15% opacity (alpha handled separately) */
#define TH_GLASS_BORDER   RGB(0xFF, 0xFF, 0xFF)   /* white tint */

/* Highlight & lighting */
#define TH_HIGHLIGHT_SHINE   RGB(0xFF, 0xFF, 0xFF)  /* 1px specular glint */
#define TH_GLOW_AMBIENT      RGB(0x55, 0x88, 0xE5)   /* blue-violet aura */

/* Buttons */
#define TH_BTN_HOVER        RGB(0x32, 0x44, 0x64)
#define TH_BTN_CLOSE        RGB(0xFF, 0x5C, 0x54)   /* ruby jewel */
#define TH_BTN_MIN          RGB(0xFF, 0xBC, 0x30)   /* amber jewel */
#define TH_BTN_MAX          RGB(0x28, 0xCD, 0x41)   /* emerald jewel */

/* Panel/taskbar */
#define TH_TASKBAR          RGB(0x10, 0x16, 0x28)   /* frosted dark bottom */
#define TH_TASKBAR_EDGE     RGB(0x38, 0x4E, 0x76)   /* 1px highlight top */

/* Terminal */
#define TH_TERM_BG          RGB(0x0C, 0x12, 0x20)   /* deep obsidian slate */
#define TH_TERM_FG          RGB(0xF4, 0xF6, 0xFF)
#define TH_TERM_CURSOR      RGB(0x58, 0xB0, 0xFF)
#define TH_TERM_PROMPT      RGB(0x58, 0xB0, 0xFF)
#define TH_TERM_OK          RGB(0x3E, 0xC8, 0x68)
#define TH_TERM_WARN        RGB(0xE5, 0xA8, 0x32)
#define TH_TERM_ERR         RGB(0xFF, 0x58, 0x50)

/* Elevation & cards */
#define TH_GLASS_CARD       RGB(0x1A, 0x25, 0x3D)   /* elevated card */
#define TH_GLASS_CARD_HDR   RGB(0x24, 0x33, 0x52)   /* card header */

/* Shadow (tint key; actual alpha set via glass.c) */
#define TH_SHADOW           RGB(0x00, 0x00, 0x00)

/* --- Corner Radii (pixels) --- */

#define RADIUS_WINDOW      12
#define RADIUS_PANEL       14
#define RADIUS_BUTTON      7
#define RADIUS_CARD        10
#define RADIUS_INPUT       6
#define RADIUS_DOCK        20
#define RADIUS_PILL        12

/* --- Opacity Aliases (0..255) --- */

#define ALPHA_WINDOW       225
#define ALPHA_PANEL        180
#define ALPHA_DOCK         165
#define ALPHA_MENU         200
#define ALPHA_NOTIFICATION 195
#define ALPHA_CARD         185
#define ALPHA_BUTTON_HOVER 195
#define ALPHA_BORDER       36   /* ~14% white opacity */
#define ALPHA_SHINE        105  /* ~41% specular glint */

/* --- Blur & Animation Flags --- */

#define BLUR_ENABLED       false
#define BLUR_RADIUS        0
#define REDUCED_MOTION     false
#define REDUCED_EFFECTS    false
/* ------------------------------------------------------------------------
 * Frosted-glass without blur
 * Design intent: Disabled blur to maintain sharpness at 1024×768 resolution.
 * Radii are tightened so UI elements remain distinct without blur softening.
 * Colors remain from the original glassmorphism palette for visual consistency.
 * ------------------------------------------------------------------------ */

/* --- DPI & HiDPI --- */

#define DPI_DEFAULT 96
#define DPI_MEDIUM  120
#define DPI_HIGH    144

/* --- vga_color_rgb for VGA mode compatibility ---
 * Maps VGA color indices to RGB, matching the old vga_to_rgb palette.
 * Used by the text terminal when rendering VGA colour cells. */
static inline color_t vga_color_rgb(int idx, bool bright) {
    (void)idx;
    (void)bright;
    return RGB(0x1A, 0x1A, 0x1A);
}

/* Note: The full implementation uses the vga_to_rgb table defined in theme.c.
 * This inline fallback prevents implicit-function-declaration errors when
 * theme.c hasn't been included yet. For the complete mapping, see theme.c. */

int  theme_get_dpi(void);
void theme_set_dpi(int dpi);
int  theme_scale(int px);

#endif