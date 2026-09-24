#include "tty.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static int active=0;
void tty_init(void){ kprintf("[TTY] init\n"); }
void tty_switch(int id){ active=id; }
int tty_get_active(void){ return active; }
