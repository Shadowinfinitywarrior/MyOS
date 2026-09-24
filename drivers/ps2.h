#pragma once
#include <stdint.h>

typedef struct {
    uint16_t data_port;
    uint16_t cmd_port;
    int initialized;
} ps2_t;

int ps2_init(void);
int ps2_keyboard_read(uint8_t *scancode);
int ps2_mouse_read(uint8_t *data);
