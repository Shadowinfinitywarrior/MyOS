#ifndef TTY_H
#define TTY_H
#include "../include/system.h"
void tty_init(void);
void tty_switch(int id);
int tty_get_active(void);
#endif
