#ifndef SCREEN_H
#define SCREEN_H
#include "../include/types.h"
void screen_init(void);
void screen_clear(void);
void screen_write(const char *str);
void screen_set_color(uint8_t fg, uint8_t bg);
void screen_putchar(char c);
int screen_get_cursor_x(void);
int screen_get_cursor_y(void);
void screen_set_cursor(int x, int y);
void screen_scroll(void);
#endif
