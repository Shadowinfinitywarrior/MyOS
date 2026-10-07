#ifndef RUST_GUI_H
#define RUST_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
typedef uint32_t color_t;
typedef uint64_t window_id_t;

/* Use existing C types from gui/rect.h and gui/surface.h */
#include "../gui/rect.h"
#include "../gui/surface.h"

/* Window structure */
typedef struct {
    window_id_t id;
    uint8_t title[64];
    rect_t frame;
    rect_t client;
    uint32_t flags;
    int visible;
    int focused;
} window_t;

/* Window flags */
#define WF_MINIMIZED  (1 << 0)
#define WF_MAXIMIZED  (1 << 1)
#define WF_RESIZING   (1 << 2)
#define WF_DRAGGING   (1 << 3)
#define WF_NO_DECOR   (1 << 4)
#define WF_MODAL      (1 << 5)

/* FFI functions exported from Rust */

/**
 * Initialize the Rust GUI system
 * @param width Screen width in pixels
 * @param height Screen height in pixels
 * @return 0 on success, negative on error
 */
int rust_gui_init(uint32_t width, uint32_t height);

/**
 * Create a new window
 * @param title Window title (null-terminated UTF-8 string)
 * @param x Initial X position
 * @param y Initial Y position
 * @param w Window width
 * @param h Window height
 * @return Window ID (0 on error)
 */
window_id_t rust_gui_create_window(const char *title, int32_t x, int32_t y,
                                   int32_t w, int32_t h);

/**
 * Destroy a window
 * @param id Window ID to destroy
 */
void rust_gui_destroy_window(window_id_t id);

/**
 * Invalidate a window (mark for redraw)
 * @param id Window ID to invalidate
 * @param x Damage region X coordinate
 * @param y Damage region Y coordinate
 * @param w Damage region width
 * @param h Damage region height
 */
void rust_gui_invalidate_window(window_id_t id, int32_t x, int32_t y, int32_t w, int32_t h);

/**
 * Render a frame (compose and display all windows)
 * @return 0 on success, negative on error
 */
int rust_gui_render_frame(void);

/**
 * Set the framebuffer pointer (called from C kernel)
 * @param fb Pointer to framebuffer memory
 * @param width Framebuffer width
 * @param height Framebuffer height
 * @param pitch Framebuffer pitch (bytes per row)
 */
void rust_gui_set_framebuffer(uint32_t *fb, uint32_t width, uint32_t height,
                               uint32_t pitch);

/**
 * Get the number of active windows
 * @return Window count
 */
int rust_gui_window_count(void);

/**
 * Get a window by index
 * @param index Window index (0-based)
 * @return Pointer to window structure, or NULL if invalid
 */
window_t *rust_gui_get_window(int32_t index);

/**
 * Focus a window
 * @param id Window ID to focus
 * @return 1 on success, 0 on failure
 */
int rust_gui_focus_window(window_id_t id);

/**
 * Set window title
 * @param id Window ID
 * @param title New title (null-terminated UTF-8 string)
 * @return 1 on success, 0 on failure
 */
int rust_gui_set_window_title(window_id_t id, const char *title);

/**
 * Get window frame rectangle
 * @param id Window ID
 * @param rect Output rectangle
 * @return 1 on success, 0 on failure
 */
int rust_gui_get_window_rect(window_id_t id, rect_t *rect);

/**
 * Set window frame rectangle (move/resize)
 * @param id Window ID
 * @param rect New rectangle
 * @return 1 on success, 0 on failure
 */
int rust_gui_set_window_rect(window_id_t id, const rect_t *rect);

/* ---- Drawing primitives (for Python Qt bindings) ---------------------------- */

/**
 * Draw a rectangle outline on a surface
 * @param surface Surface pointer
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color ARGB color
 * @param width Line width
 */
void rust_gui_draw_rect(void *surface, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t width);

/**
 * Fill a rectangle on a surface
 * @param surface Surface pointer
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color ARGB color
 */
void rust_gui_fill_rect(void *surface, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/**
 * Draw a line on a surface
 * @param surface Surface pointer
 * @param x1 Start X
 * @param y1 Start Y
 * @param x2 End X
 * @param y2 End Y
 * @param color ARGB color
 * @param width Line width
 */
void rust_gui_draw_line(void *surface, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t width);

/**
 * Draw text on a surface
 * @param surface Surface pointer
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Null-terminated UTF-8 string
 * @param color ARGB color
 * @param font_size Font size in points
 */
void rust_gui_draw_text_on_surface(void *surface, int32_t x, int32_t y, const char *text, uint32_t color, int32_t font_size);

/**
 * Draw an ellipse on a surface
 * @param surface Surface pointer
 * @param cx Center X
 * @param cy Center Y
 * @param rx Radius X
 * @param ry Radius Y
 * @param color ARGB color
 * @param width Line width
 */
void rust_gui_draw_ellipse(void *surface, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t color, int32_t width);

/**
 * Set focus to a window
 * @param id Window ID
 * @return 1 on success, 0 on failure
 */
int rust_gui_set_focus(window_id_t id);

/* ---- Surface management (for Go shell) ------------------------------------ */

/**
 * Create a new drawing surface
 * @param width Surface width
 * @param height Surface height
 * @return Pointer to surface, or NULL on error
 */
void *rust_gui_create_surface(int32_t width, int32_t height);

/**
 * Blit a surface to the framebuffer
 * @param surface Surface pointer returned by rust_gui_create_surface
 * @param x Destination X coordinate
 * @param y Destination Y coordinate
 * @return 0 on success, negative on error
 */
int rust_gui_blit_surface(void *surface, int32_t x, int32_t y);

/**
 * Get framebuffer info
 * @param width Output width
 * @param height Output height
 * @param pitch Output pitch
 */
void rust_gui_get_fb_info(int32_t *width, int32_t *height, int32_t *pitch);

/* ---- Test functions (for verification) ----------------------------------- */

/**
 * Test: Clear screen with color
 * @param color Color to fill with
 */
void rust_gui_test_clear(color_t color);

/**
 * Test: Draw a test pattern on screen
 */
void rust_gui_test_pattern(void);

/* ---- Input event handling (called from C drivers) ------------------------ */

/**
 * Push a keyboard event to the Rust GUI event queue
 * @param event_type Event type (1=down, 2=up, 3=repeat)
 * @param scancode Hardware scancode
 * @param ascii ASCII character (0 if none)
 * @param modifiers Modifier flags (shift, ctrl, alt, etc.)
 */
void rust_gui_push_key_event(uint8_t event_type, uint8_t scancode,
                             uint8_t ascii, uint32_t modifiers);

/**
 * Push a mouse event to the Rust GUI event queue
 * @param event_type Event type (4=move, 5=down, 6=up, 7=scroll_v, 8=scroll_h)
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param button Button number (1=left, 2=middle, 3=right)
 * @param delta Scroll delta (for scroll events)
 */
void rust_gui_push_mouse_event(uint8_t event_type, int32_t x, int32_t y,
                               uint8_t button, int32_t delta);

/* ---- Font initialization ---------------------------------------------------- */

/**
 * Initialize font data from C baked fonts
 * Called after rust_gui_init to provide font bitmap data
 */
void rust_gui_init_fonts(
    const uint16_t *ui_gw, const uint16_t *ui_gh, const int8_t *ui_xo, const int8_t *ui_yo,
    const uint16_t *ui_xa, const uint32_t *ui_bo, const uint8_t *ui_bitmap, uint32_t ui_bitmap_size,
    const uint16_t *mono_gw, const uint16_t *mono_gh, const int8_t *mono_xo, const int8_t *mono_yo,
    const uint16_t *mono_xa, const uint32_t *mono_bo, const uint8_t *mono_bitmap, uint32_t mono_bitmap_size,
    const uint16_t *bold_gw, const uint16_t *bold_gh, const int8_t *bold_xo, const int8_t *bold_yo,
    const uint16_t *bold_xa, const uint32_t *bold_bo, const uint8_t *bold_bitmap, uint32_t bold_bitmap_size,
    const uint16_t *blocks_gw, const uint16_t *blocks_gh, const int8_t *blocks_xo, const int8_t *blocks_yo,
    const uint16_t *blocks_xa, const uint32_t *blocks_bo, const uint8_t *blocks_bitmap, uint32_t blocks_bitmap_size
);

/* ---- Desktop shell functions ----------------------------------------------- */

/**
 * Initialize the Rust desktop shell
 * @param width Screen width
 * @param height Screen height
 */
void rust_gui_init_desktop(int32_t width, int32_t height);

/**
 * Compose the full desktop frame (wallpaper, taskbar, icons, windows)
 */
void rust_gui_compose_desktop(void);

/**
 * Handle mouse move for desktop shell
 * @param x Mouse X position
 * @param y Mouse Y position
 */
void rust_gui_desktop_mouse_move(int32_t x, int32_t y);

/**
 * Handle mouse click for desktop shell
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param button Button number (1=left, 2=middle, 3=right)
 */
void rust_gui_desktop_mouse_click(int32_t x, int32_t y, uint8_t button);

/**
 * Handle keyboard input for desktop shell
 * @param key Scancode
 * @param pressed True if key pressed, false if released
 */
void rust_gui_desktop_key(uint8_t key, bool pressed);

/* ---- External C functions needed by Rust ----------------------------------- */

uint64_t timer_get_ticks(void);
void acpi_shutdown(void);
void acpi_reboot(void);

#ifdef __cplusplus
}
#endif

#endif /* RUST_GUI_H */
