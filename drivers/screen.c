#include "screen.h"
#include "../include/system.h"
#include "../drivers/framebuffer.h"
#include "../lib/printf.h"
#include "../kernel/vtty.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define COM1 0x3F8

static uint16_t *vga = (uint16_t*)0xB8000;
static uint8_t col = 0x0F;
static int cursor_x = 0;
static int cursor_y = 0;
static vtty_t *console;

static inline uint16_t entry(char c, uint8_t color){ return (uint16_t)c | ((uint16_t)color<<8); }
static inline void serial_putc(char c){
    uint32_t tries = 100000;
    while (!(inb(COM1+5) & 0x20) && --tries);
    outb(COM1, c);
}

void screen_init(void){
    /* The boot console is virtual terminal 0. Allocating it here (rather than
     * in kernel_main) keeps the console self-contained: screen_putchar, the
     * syscall layer and the GUI all reach the same vty through console_boot(). */
    console = console_boot();
    if (!console) {
        vtty_init();
        console = console_boot();
    }
    if (console) {
        vtty_clear(console);
        vtty_set_color(console, VGA_WHITE, VGA_BLACK);
    }
    for (int i = 0; i < 80*25; i++) vga[i] = entry(' ', col);
    cursor_x = 0; cursor_y = 0;
}

void screen_clear(void){
    if (console) vtty_clear(console);
    for (int i = 0; i < 80*25; i++) vga[i] = entry(' ', col);
    cursor_x = 0; cursor_y = 0;
}

void screen_set_color(uint8_t fg, uint8_t bg){
    col = fg | (bg << 4);
    if (console) vtty_set_color(console, (uint8_t)(fg & 0x0F), (uint8_t)((bg >> 4) & 0x07));
}

void screen_scroll(void){
    if (console) { vtty_scroll(console); vtty_render_vga(console); }
    else {
        for (int row = 1; row < 25; row++)
            for (int i = 0; i < 80; i++)
                vga[(row-1)*80+i] = vga[row*80+i];
        for (int i = 0; i < 80; i++) vga[24*80+i] = entry(' ', col);
    }
    cursor_y = 24;
}

/* Put a character on the console: into the boot vty (which the VGA mirror and
 * the serial mirror follow), and onto the wire. */
void screen_putchar(char c){
    serial_putc(c);
    if (console) {
        vtty_putc(console, c);
        /* The first 80x25 cells are mirrored to VGA so the text console stays
         * authoritative for a headless boot. */
        vtty_render_vga(console);
        cursor_x = console->cx;
        cursor_y = console->cy;
    } else {
        if (c == '\n'){ cursor_x = 0; cursor_y++; }
        else if (c == '\r'){ cursor_x = 0; }
        else if (c == '\t'){ cursor_x = (cursor_x + 8) & ~7; }
        else if (c == '\b'){
            if (cursor_x > 0) cursor_x--;
            vga[cursor_y*80+cursor_x] = entry(' ', col);
        } else {
            vga[cursor_y*80+cursor_x] = entry(c, col);
            cursor_x++;
            if (cursor_x >= 80){ cursor_x = 0; cursor_y++; }
        }
        if (cursor_y >= 25) screen_scroll();
    }
}

void screen_sync_vga(void){
    if (console) vtty_render_vga(console);
}

int screen_get_cursor_x(void){ return cursor_x; }
int screen_get_cursor_y(void){ return cursor_y; }
void screen_set_cursor(int x, int y){
    if(x < 0) x = 0;
    if(x >= 80) x = 79;
    if(y < 0) y = 0;
    if(y >= 25) y = 24;
    cursor_x = x; cursor_y = y;
    if (console) vtty_move(console, x, y);
}

void screen_write(const char *s){ while(*s) screen_putchar(*s++); }
