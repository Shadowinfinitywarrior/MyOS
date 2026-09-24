#ifndef SERIAL_H
#define SERIAL_H
#include "../include/system.h"

#define COM1 0x3F8

void serial_init(int com);
void serial_write(char c);
int serial_read(void);
bool serial_is_transmit_empty(void);

#endif
