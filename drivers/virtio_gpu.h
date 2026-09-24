#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H
#include "../include/types.h"
void virtio_gpu_init(void);
void virtio_gpu_set_mode(uint32_t w,uint32_t h);
void virtio_gpu_flush(uint32_t x,uint32_t y,uint32_t w,uint32_t h);
uint32_t* virtio_gpu_get_framebuffer(void);
uint32_t virtio_gpu_get_width(void);
uint32_t virtio_gpu_get_height(void);
#endif
