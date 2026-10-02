#include "input.h"
#include "mouse.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../include/system.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define ALPS_ID_V1 0x33
#define ALPS_ID_V2 0x41
#define ALPS_ID_V3 0x42
#define ALPS_ID_V4 0x43
#define ALPS_ID_V5 0x50
#define ALPS_ID_V6 0x51
#define ALPS_ID_V7 0x52

#define ALPS_CMD_RESET           0xFF
#define ALPS_CMD_SET_SAMPLE_RATE 0xF3
#define ALPS_CMD_GET_DEVICE_ID   0xF2
#define ALPS_CMD_SET_RESOLUTION  0xE8
#define ALPS_CMD_ENABLE          0xF4
#define ALPS_CMD_DISABLE         0xF5
#define ALPS_CMD_SET_DEFAULTS    0xF6

#define ALPS_SPECIAL_CMD         0xE8
#define ALPS_QUERY_INFO          0x00
#define ALPS_SET_MODE            0x01
#define ALPS_QUERY_CAPS          0x02

#define ALPS_MODE_RELATIVE       0x00
#define ALPS_MODE_ABSOLUTE       0x01
#define ALPS_MODE_MULTI_FINGER   0x02
#define ALPS_MODE_GESTURE        0x04
#define ALPS_MODE_TAP_CLICK      0x08

#define ALPS_ACK 0xFA
#define ALPS_NACK 0xFE
#define ALPS_ERROR 0xFC

typedef struct {
    uint8_t version;
    uint16_t min_x, max_x;
    uint16_t min_y, max_y;
    uint16_t min_pressure, max_pressure;
    bool has_multi_finger;
    bool has_gesture;
    bool absolute_mode;
    uint8_t mode_byte;
    uint8_t button_state;
    int16_t last_x, last_y;
    uint16_t last_z;
    uint8_t finger_count;
} alps_state_t;

static alps_state_t g_alps = {0};
static volatile uint8_t alps_packet[8];
static volatile uint8_t alps_cycle = 0;
static volatile uint8_t alps_expected_bytes = 6;
static volatile bool alps_awaiting_ack = false;

static void alps_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && (inb(MOUSE_PORT_CMD) & 0x02));
}

static void alps_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(MOUSE_PORT_CMD) & 0x01));
}

static bool alps_write_cmd(uint8_t cmd) {
    alps_wait_output();
    outb(MOUSE_PORT_CMD, 0xD4);
    alps_wait_output();
    outb(MOUSE_PORT_DATA, cmd);
    alps_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == ALPS_ACK;
}

static bool alps_write_cmd_arg(uint8_t cmd, uint8_t arg) {
    if (!alps_write_cmd(cmd)) return false;
    alps_wait_output();
    outb(MOUSE_PORT_DATA, arg);
    alps_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == ALPS_ACK;
}

static uint8_t alps_read_data(void) {
    alps_wait_input();
    return inb(MOUSE_PORT_DATA);
}

static bool alps_special_cmd(uint8_t subcmd, uint8_t arg) {
    if (!alps_write_cmd(ALPS_SPECIAL_CMD)) return false;
    if (!alps_write_cmd_arg(subcmd, arg)) return false;
    return true;
}

static uint8_t alps_query_byte(uint8_t subcmd) {
    alps_write_cmd(ALPS_SPECIAL_CMD);
    alps_write_cmd_arg(subcmd, 0);
    return alps_read_data();
}

static void alps_decode_packet_v1_v2(volatile uint8_t *packet, input_event_t *event) {
    uint8_t byte0 = packet[0];
    uint8_t byte1 = packet[1];
    uint8_t byte2 = packet[2];
    uint8_t byte3 = packet[3];
    uint8_t byte4 = packet[4];
    uint8_t byte5 = packet[5];
    
    bool left_btn = (byte0 & 0x01) != 0;
    bool right_btn = (byte0 & 0x02) != 0;
    bool middle_btn = (byte0 & 0x04) != 0;
    
    uint16_t x = ((byte1 & 0x0F) << 8) | byte2;
    uint16_t y = ((byte3 & 0x0F) << 8) | byte4;
    uint8_t z = byte5 & 0x7F;
    
    uint8_t buttons = 0;
    if (left_btn) buttons |= 1;
    if (right_btn) buttons |= 2;
    if (middle_btn) buttons |= 4;
    
    uint8_t changed = buttons ^ g_alps.button_state;
    if (changed) {
        for (int b = 0; b < 3; b++) {
            if (changed & (1 << b)) {
                event->type = (buttons & (1 << b)) ? INPUT_EVENT_BUTTON_DOWN : INPUT_EVENT_BUTTON_UP;
                event->button = b;
                event->x = x;
                event->y = g_alps.max_y - y;
                event->timestamp = timer_get_ticks();
                input_queue_event(event);
            }
        }
        g_alps.button_state = buttons;
    }
    
    if (g_alps.absolute_mode) {
        event->type = INPUT_EVENT_ABSOLUTE_MOVE;
        event->x = x;
        event->y = g_alps.max_y - y;
        event->dx = x - g_alps.last_x;
        event->dy = (g_alps.max_y - y) - g_alps.last_y;
        g_alps.last_x = x;
        g_alps.last_y = g_alps.max_y - y;
    } else {
        int16_t dx = (int8_t)byte1;
        int16_t dy = (int8_t)byte3;
        event->type = INPUT_EVENT_MOVE;
        event->dx = dx;
        event->dy = -dy;
    }
    
    if (z != g_alps.last_z) {
        if (z > 0 && g_alps.last_z == 0) {
            event->type = INPUT_EVENT_TOUCH_DOWN;
            event->touches[0].x = x;
            event->touches[0].y = g_alps.max_y - y;
            event->touches[0].pressure = z;
            event->touch_count = 1;
        } else if (z == 0 && g_alps.last_z > 0) {
            event->type = INPUT_EVENT_TOUCH_UP;
            event->touch_count = 0;
        }
        g_alps.last_z = z;
    }
    
    event->timestamp = timer_get_ticks();
    input_queue_event(event);
}

static void alps_decode_packet_v3_v4(volatile uint8_t *packet, input_event_t *event) {
    uint8_t byte0 = packet[0];
    uint8_t byte1 = packet[1];
    uint8_t byte2 = packet[2];
    uint8_t byte3 = packet[3];
    uint8_t byte4 = packet[4];
    uint8_t byte5 = packet[5];
    uint8_t byte6 = packet[6];
    
    bool left_btn = (byte0 & 0x01) != 0;
    bool right_btn = (byte0 & 0x02) != 0;
    
    uint16_t x = ((byte1 & 0x0F) << 8) | byte2;
    uint16_t y = ((byte3 & 0x0F) << 8) | byte4;
    uint8_t z = byte5 & 0x7F;
    
    uint8_t finger_state = byte6;
    uint8_t finger_count = finger_state & 0x07;
    
    uint8_t buttons = 0;
    if (left_btn) buttons |= 1;
    if (right_btn) buttons |= 2;
    
    uint8_t changed = buttons ^ g_alps.button_state;
    if (changed) {
        for (int b = 0; b < 2; b++) {
            if (changed & (1 << b)) {
                event->type = (buttons & (1 << b)) ? INPUT_EVENT_BUTTON_DOWN : INPUT_EVENT_BUTTON_UP;
                event->button = b;
                event->x = x;
                event->y = g_alps.max_y - y;
                event->timestamp = timer_get_ticks();
                input_queue_event(event);
            }
        }
        g_alps.button_state = buttons;
    }
    
    event->type = INPUT_EVENT_ABSOLUTE_MOVE;
    event->x = x;
    event->y = g_alps.max_y - y;
    event->dx = x - g_alps.last_x;
    event->dy = (g_alps.max_y - y) - g_alps.last_y;
    g_alps.last_x = x;
    g_alps.last_y = g_alps.max_y - y;
    
    if (z != g_alps.last_z) {
        if (z > 0 && g_alps.last_z == 0) {
            event->type = INPUT_EVENT_TOUCH_DOWN;
            event->touches[0].x = x;
            event->touches[0].y = g_alps.max_y - y;
            event->touches[0].pressure = z;
            event->touch_count = finger_count;
        } else if (z == 0 && g_alps.last_z > 0) {
            event->type = INPUT_EVENT_TOUCH_UP;
            event->touch_count = 0;
        }
        g_alps.last_z = z;
    }
    
    g_alps.finger_count = finger_count;
    event->touch_count = finger_count;
    
    if (finger_count == 2) {
        event->type = INPUT_EVENT_GESTURE;
        event->gesture = GESTURE_SCROLL_TWO_FINGER;
    } else if (finger_count == 3) {
        event->type = INPUT_EVENT_GESTURE;
        event->gesture = GESTURE_THREE_FINGER_TAP;
    }
    
    event->timestamp = timer_get_ticks();
    input_queue_event(event);
}

static void alps_init(input_device_t *dev) {
    kprintf("[ALPS] Initializing ALPS TouchPad...\n");
    
    dev->min_x = 0;
    dev->max_x = 1023;
    dev->min_y = 0;
    dev->max_y = 767;
    
    alps_write_cmd(ALPS_CMD_RESET);
    alps_read_data();
    alps_read_data();
    
    alps_write_cmd(ALPS_CMD_SET_DEFAULTS);
    alps_write_cmd(ALPS_CMD_SET_SAMPLE_RATE);
    alps_write_cmd(80);
    
    alps_write_cmd(ALPS_CMD_GET_DEVICE_ID);
    alps_read_data();
    uint8_t device_id = alps_read_data();
    kprintf("[ALPS] Device ID: 0x%02X\n", device_id);
    
    if (device_id == ALPS_ID_V1 || device_id == ALPS_ID_V2) {
        g_alps.version = 1;
        alps_expected_bytes = 6;
    } else if (device_id == ALPS_ID_V3 || device_id == ALPS_ID_V4) {
        g_alps.version = 3;
        alps_expected_bytes = 8;
    } else if (device_id == ALPS_ID_V5 || device_id == ALPS_ID_V6 || device_id == ALPS_ID_V7) {
        g_alps.version = 5;
        alps_expected_bytes = 8;
    } else {
        kprintf("[ALPS] Unknown ALPS version: 0x%02X\n", device_id);
        return;
    }
    
    uint8_t caps = alps_query_byte(ALPS_QUERY_CAPS);
    kprintf("[ALPS] Capabilities: 0x%02X\n", caps);
    
    g_alps.has_multi_finger = (caps & 0x02) != 0;
    g_alps.has_gesture = (caps & 0x04) != 0;
    
    dev->absolute_mode = true;
    g_alps.absolute_mode = true;
    
    alps_special_cmd(ALPS_SET_MODE, ALPS_MODE_ABSOLUTE | ALPS_MODE_MULTI_FINGER | ALPS_MODE_GESTURE | ALPS_MODE_TAP_CLICK);
    
    dev->min_x = 0;
    dev->max_x = 2047;
    dev->min_y = 0;
    dev->max_y = 1535;
    
    alps_write_cmd(ALPS_CMD_ENABLE);
    
    dev->present = true;
    kprintf("[ALPS] TouchPad initialized (v%d, absolute mode, multi-finger: %s)\n",
            g_alps.version, g_alps.has_multi_finger ? "yes" : "no");
}

static void alps_poll(input_device_t *dev) {
    (void)dev;
    
    uint8_t status = inb(MOUSE_PORT_CMD);
    if (!(status & 0x01)) return;
    if (!(status & 0x20)) {
        inb(MOUSE_PORT_DATA);
        return;
    }
    
    uint8_t data = inb(MOUSE_PORT_DATA);
    
    if (alps_awaiting_ack) {
        if (data == ALPS_ACK) {
            alps_awaiting_ack = false;
        }
        return;
    }
    
    if (alps_cycle == 0) {
        if (g_alps.version >= 3) {
            if (!(data & 0x80)) return;
        } else {
            if (!(data & 0x08)) return;
        }
        alps_packet[0] = data;
        alps_cycle = 1;
        return;
    }
    
    if (alps_cycle < alps_expected_bytes) {
        alps_packet[alps_cycle] = data;
        alps_cycle++;
    }
    
    if (alps_cycle == alps_expected_bytes) {
        alps_cycle = 0;
        input_event_t event = {0};
        if (g_alps.version >= 3) {
            alps_decode_packet_v3_v4(alps_packet, &event);
        } else {
            alps_decode_packet_v1_v2(alps_packet, &event);
        }
    }
}

static void alps_enable(input_device_t *dev) {
    (void)dev;
    alps_write_cmd(ALPS_CMD_ENABLE);
}

static void alps_disable(input_device_t *dev) {
    (void)dev;
    alps_write_cmd(ALPS_CMD_DISABLE);
}

static bool alps_handle_irq(input_device_t *dev, uint8_t data) {
    (void)dev; (void)data;
    return false;
}

static void alps_set_resolution(input_device_t *dev, uint16_t x, uint16_t y) {
    (void)dev; (void)x; (void)y;
}

static void alps_set_scaling(input_device_t *dev, bool one_to_one) {
    (void)dev; (void)one_to_one;
}

static input_device_t g_alps_dev = {
    .type = INPUT_DEVICE_ALPS,
    .name = "ALPS TouchPad",
    .present = false,
    .absolute_mode = true,
    .init = alps_init,
    .poll = alps_poll,
    .enable = alps_enable,
    .disable = alps_disable,
    .handle_irq = alps_handle_irq,
    .set_resolution = alps_set_resolution,
    .set_scaling = alps_set_scaling,
};

input_device_t *alps_create(void) {
    return &g_alps_dev;
}