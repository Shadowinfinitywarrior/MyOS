#ifndef GUI_GLASS_H
#define GUI_GLASS_H

#include "blit.h"
#include "rect.h"
#include "surface.h"
#include "text.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * MyOS Centralized Glassmorphism Design System Tokens
 * ========================================================================= */

typedef struct {
    /* Color Palette */
    color_t bg_deep;                /* Desktop canvas background: #0C101D */
    color_t bg_mid;                 /* Mid cosmic sapphire: #151A30 */
    color_t glass_surface_primary;  /* Primary glass surface: #171D2B */
    color_t glass_surface_sec;      /* Secondary glass surface: #222A3B */
    color_t accent_primary;         /* Primary accent: #8B9DFF */
    color_t accent_secondary;       /* Secondary accent: #65D9FF */
    color_t text_primary;           /* Primary text: #F4F6FF */
    color_t text_secondary;         /* Secondary text: #A8B0C5 */
    color_t text_dim;               /* Dim text: #6E7C98 */
    color_t border_glass;           /* Glass border tint: white 10-15% */
    color_t highlight_shine;        /* Upper edge specular shine */
    color_t glow_ambient;           /* Ambient blue-violet aura */

    /* Corner Radii */
    int radius_window;              /* 16 px */
    int radius_panel;               /* 18 px */
    int radius_button;              /* 9 px */
    int radius_card;                /* 12 px */
    int radius_input;               /* 8 px */
    int radius_dock;                /* 24 px */
    int radius_pill;                /* 14 px */

    /* Translucency & Opacities (0..255) */
    uint8_t alpha_window;           /* 225 */
    uint8_t alpha_panel;            /* 180 */
    uint8_t alpha_dock;             /* 165 */
    uint8_t alpha_menu;             /* 200 */
    uint8_t alpha_notification;     /* 195 */
    uint8_t alpha_card;             /* 185 */
    uint8_t alpha_button_hover;     /* 195 */
    uint8_t alpha_border;           /* 36 (14% white opacity) */
    uint8_t alpha_shine;            /* 105 (41% specular glint) */

    /* Lighting, Blur & Animation Flags */
    bool blur_enabled;              /* Backdrop blur active */
    int  blur_radius;               /* Blur sampling step */
    bool reduced_motion;            /* Accessibility reduced motion */
    bool reduced_effects;           /* Low-power / CPU fallback mode */
} glass_theme_t;

extern glass_theme_t MyOSTheme;

/* Theme & Configuration API */
void glass_init(void);
void glass_set_blur_enabled(bool enabled);
bool glass_get_blur_enabled(void);
void glass_set_reduced_motion(bool enabled);
bool glass_get_reduced_motion(void);
void glass_set_reduced_effects(bool enabled);
bool glass_get_reduced_effects(void);

/* Backbuffer backdrop frosted-blur filter */
void glass_backdrop_blur(uint32_t *bb, int stride, const rect_t *r, int radius);

/* Reusable Glass Primitives on Backbuffer */
void glass_render_surface(uint32_t *bb, int stride, const rect_t *r,
                          int radius, color_t tint, uint8_t alpha,
                          bool with_blur, bool with_shadow, bool with_specular);

void glass_render_panel(uint32_t *bb, int stride, const rect_t *r);

void glass_render_button(uint32_t *bb, int stride, const rect_t *r,
                         const char *label, const baked_font_t *font,
                         bool hover, bool active, color_t text_color);

void glass_render_notification(uint32_t *bb, int stride,
                               int x, int y, int w, int h,
                               const char *title, const char *msg,
                               color_t accent);

void glass_render_tooltip(uint32_t *bb, int stride,
                          int x, int y, const char *text);

/* Reusable Glass Primitives on Surface */
void glass_surface_card(surface_t *s, const rect_t *area, const char *title);

void glass_surface_button(surface_t *s, const rect_t *r,
                          const char *label, const baked_font_t *font,
                          bool hover, bool active, color_t text_color);

void glass_surface_textfield(surface_t *s, const rect_t *r,
                             const char *text, const char *placeholder,
                             bool focused);

#ifdef __cplusplus
}
#endif

#endif /* GUI_GLASS_H */
