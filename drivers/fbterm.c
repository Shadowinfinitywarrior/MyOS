#include "fbterm.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static char screen[FBTERM_MAX_ROWS][FBTERM_MAX_COLS];
static uint32_t cols,rows,cx,cy;
static bool active=false;
void fbterm_init(uint32_t w,uint32_t h){ cols=w/8; rows=h/10; if(cols>FBTERM_MAX_COLS) cols=FBTERM_MAX_COLS; if(rows>FBTERM_MAX_ROWS) rows=FBTERM_MAX_ROWS; active=true; fbterm_clear(); kprintf("[FBTERM] %ux%u\n",cols,rows); }
void fbterm_clear(void){ for(uint32_t y=0;y<rows;y++) for(uint32_t x=0;x<cols;x++) screen[y][x]=' '; cx=cy=0; }
void fbterm_putchar(char c){ if(!active) return; if(c=='\n'){ cx=0; cy++; } else if(c>=' ') { if(cx>=cols){cx=0;cy++;} if(cy<rows&&cx<cols) screen[cy][cx++]=c; } }
void fbterm_write(const char* s){ while(*s) fbterm_putchar(*s++); }
void fbterm_set_active(bool a){ active=a; }
void fbterm_draw(void){}
