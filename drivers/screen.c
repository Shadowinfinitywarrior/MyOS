#include "screen.h"
#include "../include/system.h"
#include "../drivers/framebuffer.h"
#include "../drivers/fbterm.h"
#include "../drivers/fbcon.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define SCREEN_COLS 80
#define SCREEN_ROWS 25
#define COM1 0x3F8

static uint16_t *vga = (uint16_t*)0xB8000;
static uint8_t col = 0x0F;
static int cursor_x = 0;
static int cursor_y = 0;
static bool fb_initialized = false;

static inline uint16_t entry(char c, uint8_t color){ return (uint16_t)c | ((uint16_t)color<<8); }
static inline void serial_putc(char c){
    uint32_t tries = 100000;
    while (!(inb(COM1+5) & 0x20) && --tries);
    outb(COM1, c);
}

void screen_init(void){
    /* Initialize framebuffer console */
    fb_info_t *fb = fb_get_info();
    if (fb->width > 0 && fb->height > 0) {
        fbcon_init(fb->width, fb->height);
        fb_initialized = true;
        kprintf("[SCREEN] Framebuffer console initialized %ux%u\n", fb->width, fb->height);
    } else {
        /* Fallback to VGA text mode */
        for(int i=0;i<SCREEN_COLS*SCREEN_ROWS;i++) vga[i]=entry(' ',col);
        cursor_x = 0; cursor_y = 0;
        fb_initialized = false;
    }
    cursor_x = 0; cursor_y = 0;
}
void screen_clear(void){ screen_init(); }
void screen_set_color(uint8_t fg, uint8_t bg){ col = fg | (bg<<4); }

void screen_scroll(void){
    if (fb_initialized) {
        fbcon_scroll();
        return;
    }
    for(int row=1; row<SCREEN_ROWS; row++)
        for(int i=0; i<SCREEN_COLS; i++)
            vga[(row-1)*SCREEN_COLS+i] = vga[row*SCREEN_COLS+i];
    for(int i=0; i<SCREEN_COLS; i++)
        vga[(SCREEN_ROWS-1)*SCREEN_COLS+i] = entry(' ',col);
    cursor_y = SCREEN_ROWS-1;
}

void screen_putchar(char c){
    serial_putc(c);
    if (fb_initialized) {
        if(c == '\n'){ cursor_x = 0; cursor_y++; }
        else if(c == '\r'){ cursor_x = 0; }
        else if(c == '\t'){ cursor_x = (cursor_x + 8) & ~7; }
        else if(c == '\b'){
            if(cursor_x > 0) cursor_x--;
        } else if(c >= ' ') {
            fbcon_putchar(c);
            cursor_x++;
        }
        if(cursor_y >= SCREEN_ROWS) screen_scroll();
    } else {
        if(c == '\n'){ cursor_x = 0; cursor_y++; }
        else if(c == '\r'){ cursor_x = 0; }
        else if(c == '\t'){ cursor_x = (cursor_x + 8) & ~7; }
        else if(c == '\b'){
            if(cursor_x > 0) cursor_x--;
            vga[cursor_y*SCREEN_COLS+cursor_x] = entry(' ',col);
        } else {
            vga[cursor_y*SCREEN_COLS+cursor_x] = entry(c,col);
            cursor_x++;
            if(cursor_x >= SCREEN_COLS){ cursor_x = 0; cursor_y++; }
        }
        if(cursor_y >= SCREEN_ROWS) screen_scroll();
    }
}

int screen_get_cursor_x(void){ return cursor_x; }
int screen_get_cursor_y(void){ return cursor_y; }
void screen_set_cursor(int x, int y){
    if(x < 0) x = 0;
    if(x >= SCREEN_COLS) x = SCREEN_COLS-1;
    if(y < 0) y = 0;
    if(y >= SCREEN_ROWS) y = SCREEN_ROWS-1;
    cursor_x = x; cursor_y = y;
}

void screen_write(const char *s){ while(*s) screen_putchar(*s++); }