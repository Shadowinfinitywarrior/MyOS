#include "theme.h"

/* The 16 ANSI colours as 0x00RRGGBB, matching what the vty cells reference. */
const color_t vga_to_rgb[16] = {
    RGB(0x1A, 0x1A, 0x1A),  /* 0 black          */
    RGB(0xE0, 0x4F, 0x5F),  /* 1 red            */
    RGB(0x5A, 0xC8, 0x7A),  /* 2 green          */
    RGB(0xE8, 0xB0, 0x4A),  /* 3 yellow         */
    RGB(0x4C, 0x8D, 0xF6),  /* 4 blue           */
    RGB(0xC0, 0x6C, 0xE8),  /* 5 magenta        */
    RGB(0x4C, 0xC5, 0xD6),  /* 6 cyan           */
    RGB(0xC8, 0xD4, 0xE4),  /* 7 white          */
    RGB(0x5A, 0x63, 0x73),  /* 8 bright black   */
    RGB(0xFF, 0x7B, 0x87),  /* 9 bright red     */
    RGB(0x7D, 0xE3, 0xA2),  /* 10 bright green  */
    RGB(0xFF, 0xD1, 0x7A),  /* 11 bright yellow */
    RGB(0x7A, 0xAE, 0xFF),  /* 12 bright blue   */
    RGB(0xDE, 0x9C, 0xFF),  /* 13 bright magenta*/
    RGB(0x7A, 0xE2, 0xF0),  /* 14 bright cyan   */
    RGB(0xFF, 0xFF, 0xFF)   /* 15 bright white  */
};

static int g_current_dpi = DPI_DEFAULT;

int theme_get_dpi(void) {
    return g_current_dpi;
}

void theme_set_dpi(int dpi) {
    if (dpi < 72) dpi = 72;
    if (dpi > 288) dpi = 288;
    g_current_dpi = dpi;
}

int theme_scale(int px) {
    return (px * g_current_dpi + 48) / 96;
}
