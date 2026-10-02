#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../include/types.h"

typedef struct fb_info {
    uint32_t phys_addr;
    uint32_t virt_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t refresh_rate;
    uint32_t frame_count;
    uint32_t last_vsync_time;
    bool vsync_enabled;
} fb_info_t;

bool        fb_detect(void);
void        fb_init(void);
void        fb_fill(uint32_t color);
void        fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t    fb_get_pixel(uint32_t x, uint32_t y);
void        fb_wait_vsync(void);
fb_info_t  *fb_get_info(void);

/* ---- Software-rendering back buffer -------------------------------------
 *
 * The LFB itself is mapped uncached (MMIO). Drawing straight into it costs an
 * uncached store per pixel, which makes anything larger than a glyph crawl.
 * Instead every primitive draws into a cached RAM back buffer, records the
 * rectangle it touched, and the compositor pushes only damaged rectangles to
 * the LFB. The same accessors are used whether or not a back buffer exists
 * (a text-mode boot with no BGA keeps fb_get_backbuffer() == NULL).
 */
uint32_t   *fb_get_backbuffer(void);      /* width*height pixels, or NULL     */
int         fb_get_stride(void);          /* back buffer pitch in pixels      */
void        fb_add_damage(int x, int y, int w, int h);
void        fb_clear_damage(void);
bool        fb_has_damage(void);
uint32_t    fb_damage_count(void);
/* Push the accumulated damage (or the whole screen) to the LFB. */
void        fb_flush(void);
void        fb_flush_all(void);

#endif
