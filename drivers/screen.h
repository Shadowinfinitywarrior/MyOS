#ifndef SCREEN_H
#define SCREEN_H
#include "../include/types.h"

/* The kernel console.
 *
 * Everything the kernel prints goes to the boot virtual terminal, which is
 * mirrored to the serial port and to the VGA text buffer so a headless boot
 * still produces readable logs. When a graphical session takes over the
 * framebuffer, the desktop owns the screen and this console becomes
 * serial/VGA only.
 */

void screen_init(void);
void screen_clear(void);
void screen_write(const char *str);
void screen_set_color(uint8_t fg, uint8_t bg);
void screen_putchar(char c);
int screen_get_cursor_x(void);
int screen_get_cursor_y(void);
void screen_set_cursor(int x, int y);
void screen_scroll(void);

/* Mirror the boot terminal's grid to VGA after it changes. */
void screen_sync_vga(void);

#endif
