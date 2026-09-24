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

bool fb_detect(void);
void fb_init(void);
void fb_fill(uint32_t color);
void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_pixel(uint32_t x, uint32_t y);
void fb_wait_vsync(void);

fb_info_t *fb_get_info(void);

#endif
