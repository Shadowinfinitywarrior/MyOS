#include "ps2.h"
#include <lib/printf.h>

static ps2_t ps2;

int ps2_init(void) {
    ps2.data_port = 0x60;
    ps2.cmd_port = 0x64;
    ps2.initialized = 0;
    kprintf("[ps2] init stub\n");
    return 0;
}

int ps2_keyboard_read(uint8_t *scancode) {
    // TODO: read from 0x60
    return -1;
}

int ps2_mouse_read(uint8_t *data) {
    // TODO: read mouse packet
    return -1;
}
