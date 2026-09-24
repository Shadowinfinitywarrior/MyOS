#ifndef FBTERM_H
#define FBTERM_H
#include "../include/types.h"
#define FBTERM_MAX_COLS 128
#define FBTERM_MAX_ROWS 64
void fbterm_init(uint32_t w,uint32_t h);
void fbterm_putchar(char c);
void fbterm_write(const char* s);
void fbterm_clear(void);
void fbterm_set_active(bool a);
void fbterm_draw(void);
#endif
