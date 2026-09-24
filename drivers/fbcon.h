#ifndef FBCON_H
#define FBCON_H
#include "../include/types.h"
void fbcon_init(uint32_t w,uint32_t h);
void fbcon_putchar(char c);
void fbcon_scroll(void);
void fbcon_write(const char *s);
void fbcon_clear(void);
void fbcon_set_color(uint32_t fg,uint32_t bg);
#endif
