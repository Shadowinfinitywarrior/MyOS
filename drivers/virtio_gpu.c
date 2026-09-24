#include "virtio_gpu.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static uint32_t width=1024,height=768;
static uint32_t* fb=NULL;
void virtio_gpu_init(void){ }
void virtio_gpu_set_mode(uint32_t w,uint32_t h){ width=w; height=h; }
void virtio_gpu_flush(uint32_t x,uint32_t y,uint32_t w,uint32_t h){ (void)x;(void)y;(void)w;(void)h; }
uint32_t* virtio_gpu_get_framebuffer(void){ return fb; }
uint32_t virtio_gpu_get_width(void){ return width; }
uint32_t virtio_gpu_get_height(void){ return height; }
