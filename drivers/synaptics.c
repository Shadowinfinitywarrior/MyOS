#include "input.h"
#include "mouse.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../include/system.h"
#include "../lib/string.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define SYNAPTICS_ID 0x32  // 0x32 = Synaptics TouchPad

#define SYNAPTICS_CMD_RESET           0xFF
#define SYNAPTICS_CMD_SET_SAMPLE_RATE 0xF3
#define SYNAPTICS_CMD_GET_DEVICE_ID   0xF2
#define SYNAPTICS_CMD_SET_RESOLUTION  0xE8
#define SYNAPTICS_CMD_SET_SCALING_1_1 0xE6
#define SYNAPTICS_CMD_SET_SCALING_2_1 0xE7
#define SYNAPTICS_CMD_ENABLE          0xF4
#define SYNAPTICS_CMD_DISABLE         0xF5
#define SYNAPTICS_CMD_SET_DEFAULTS    0xF6

#define SYNAPTICS_SPECIAL_CMD         0xE9  // Special Synaptics command
#define SYNAPTICS_QUERY_MODE          0x00
#define SYNAPTICS_SET_MODE            0x01
#define SYNAPTICS_QUERY_INFO          0x02
#define SYNAPTICS_SET_RESOLUTION      0x03
#define SYNAPTICS_QUERY_RESOLUTION    0x04
#define SYNAPTICS_SET_SAMPLING_RATE   0x05
#define SYNAPTICS_QUERY_SAMPLING      0x06

#define SYNAPTICS_MODE_RELATIVE       0x00
#define SYNAPTICS_MODE_ABSOLUTE       0x01
#define SYNAPTICS_MODE_MULTI_FINGER   0x02
#define SYNAPTICS_MODE_GESTURE        0x04
#define SYNAPTICS_MODE_TAP_CLICK      0x08

#define SYNAPTICS_ACK 0xFA
#define SYNAPTICS_NACK 0xFE
#define SYNAPTICS_ERROR 0xFC

typedef struct {
    uint16_t min_x, max_x;
    uint16_t min_y, max_y;
    uint16_t min_pressure, max_pressure;
    uint16_t finger_width;
    bool has_extended_buttons;
    bool has_multi_finger;
    bool has_palm_detect;
    bool absolute_mode;
    uint8_t mode_byte;
    uint8_t button_state;
    int16_t last_x, last_y;
    uint16_t last_z;
    uint8_t finger_count;
    uint32_t last_tap_time;
    int16_t last_tap_x, last_tap_y;
    uint8_t tap_count;
} synaptics_state_t;

static synaptics_state_t g_synaptics = {0};
static volatile uint8_t synaptics_packet[6];
static volatile uint8_t synaptics_cycle = 0;
static volatile bool synaptics_awaiting_ack = false;

static void synaptics_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && (inb(MOUSE_PORT_CMD) & 0x02));
}

static void synaptics_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(MOUSE_PORT_CMD) & 0x01));
}

static bool synaptics_write_cmd(uint8_t cmd) {
    synaptics_wait_output();
    outb(MOUSE_PORT_CMD, 0xD4);
    synaptics_wait_output();
    outb(MOUSE_PORT_DATA, cmd);
    synaptics_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == SYNAPTICS_ACK;
}

static bool synaptics_write_cmd_arg(uint8_t cmd, uint8_t arg) {
    if (!synaptics_write_cmd(cmd)) return false;
    synaptics_wait_output();
    outb(MOUSE_PORT_DATA, arg);
    synaptics_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == SYNAPTICS_ACK;
}

static uint8_t synaptics_read_data(void) {
    synaptics_wait_input();
    return inb(MOUSE_PORT_DATA);
}

static bool synaptics_special_cmd(uint8_t subcmd, uint8_t arg) {
    if (!synaptics_write_cmd(SYNAPTICS_SPECIAL_CMD)) return false;
    if (!synaptics_write_cmd_arg(subcmd, arg)) return false;
    return true;
}

static uint8_t synaptics_query_byte(uint8_t subcmd) {
    synaptics_write_cmd(SYNAPTICS_SPECIAL_CMD);
    synaptics_write_cmd_arg(subcmd, 0);
    return synaptics_read_data();
}

static int abs_int(int x) { return x < 0 ? -x : x; }

static void synaptics_decode_packet(volatile uint8_t *packet, input_event_t *event) {
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
    
    uint8_t changed = buttons ^ g_synaptics.button_state;
    if (changed) {
        for (int b = 0; b < 3; b++) {
            if (changed & (1 << b)) {
                if (buttons & (1 << b)) {
                    event->type = INPUT_EVENT_BUTTON_DOWN;
                    event->button = b;
                } else {
                    event->type = INPUT_EVENT_BUTTON_UP;
                    event->button = b;
                }
                event->x = x;
                event->y = g_synaptics.max_y - y;
                event->timestamp = timer_get_ticks();
                input_queue_event(event);
            }
        }
        g_synaptics.button_state = buttons;
    }
    
    if (g_synaptics.absolute_mode) {
        event->type = INPUT_EVENT_ABSOLUTE_MOVE;
        event->x = x;
        event->y = g_synaptics.max_y - y;
        event->dx = x - g_synaptics.last_x;
        event->dy = (g_synaptics.max_y - y) - g_synaptics.last_y;
        g_synaptics.last_x = x;
        g_synaptics.last_y = g_synaptics.max_y - y;
    } else {
        int16_t dx = (int16_t)(byte1 & 0x0F) | ((byte0 & 0x20) ? 0xF0 : 0);
        int16_t dy = (int16_t)(byte2 & 0x0F) | ((byte0 & 0x40) ? 0xF0 : 0);
        
        event->type = INPUT_EVENT_MOVE;
        event->dx = dx;
        event->dy = -dy;
    }
    
    if (z != g_synaptics.last_z) {
        if (z > 0 && g_synaptics.last_z == 0) {
            event->type = INPUT_EVENT_TOUCH_DOWN;
            event->touches[0].x = x;
            event->touches[0].y = g_synaptics.max_y - y;
            event->touches[0].pressure = z;
            event->touches[0].finger_count = 1;
            event->touch_count = 1;
        } else if (z == 0 && g_synaptics.last_z > 0) {
            event->type = INPUT_EVENT_TOUCH_UP;
            event->touches[0].x = g_synaptics.last_x;
            event->touches[0].y = g_synaptics.last_y;
            event->touch_count = 0;
            
            uint32_t now = timer_get_ticks();
            if (now - g_synaptics.last_tap_time < 300 &&
                abs_int(x - g_synaptics.last_tap_x) < 20 &&
                abs_int(y - g_synaptics.last_tap_y) < 20) {
                g_synaptics.tap_count++;
                if (g_synaptics.tap_count == 2) {
                    event->type = INPUT_EVENT_GESTURE;
                    event->gesture = GESTURE_DOUBLE_TAP;
                }
            } else {
                g_synaptics.tap_count = 1;
            }
            g_synaptics.last_tap_time = now;
            g_synaptics.last_tap_x = x;
            g_synaptics.last_tap_y = y;
        }
        g_synaptics.last_z = z;
    }
    
    if (g_synaptics.has_multi_finger) {
        uint8_t finger_info = synaptics_query_byte(0x0A);
        g_synaptics.finger_count = finger_info & 0x07;
        event->touch_count = g_synaptics.finger_count;
        
        if (g_synaptics.finger_count == 2) {
            event->type = INPUT_EVENT_GESTURE;
            event->gesture = GESTURE_SCROLL_TWO_FINGER;
        } else if (g_synaptics.finger_count == 3) {
            event->type = INPUT_EVENT_GESTURE;
            event->gesture = GESTURE_THREE_FINGER_TAP;
        }
    }
    
    event->timestamp = timer_get_ticks();
    input_queue_event(event);
}

static void synaptics_init(input_device_t *dev) {
    kprintf("[SYNAPTICS] Initializing Synaptics TouchPad...\n");
    
    dev->min_x = 0;
    dev->max_x = 1023;
    dev->min_y = 0;
    dev->max_y = 767;
    dev->min_pressure = 0;
    dev->max_pressure = 255;
    
    synaptics_write_cmd(SYNAPTICS_CMD_RESET);
    synaptics_read_data();
    synaptics_read_data();
    
    synaptics_write_cmd(SYNAPTICS_CMD_SET_DEFAULTS);
    
    synaptics_write_cmd(SYNAPTICS_CMD_SET_SAMPLE_RATE);
    synaptics_write_cmd(100);
    
    synaptics_write_cmd(SYNAPTICS_CMD_GET_DEVICE_ID);
    synaptics_read_data();
    uint8_t device_id = synaptics_read_data();
    kprintf("[SYNAPTICS] Device ID: 0x%02X\n", device_id);
    
    if (device_id == SYNAPTICS_ID) {
        uint8_t info = synaptics_query_byte(SYNAPTICS_QUERY_INFO);
        kprintf("[SYNAPTICS] Info: 0x%02X\n", info);
        
        g_synaptics.has_extended_buttons = (info & 0x01) != 0;
        g_synaptics.has_multi_finger = (info & 0x02) != 0;
        g_synaptics.has_palm_detect = (info & 0x04) != 0;
        
        dev->absolute_mode = true;
        g_synaptics.absolute_mode = true;
        
        synaptics_special_cmd(SYNAPTICS_SET_MODE, 
            SYNAPTICS_MODE_ABSOLUTE | SYNAPTICS_MODE_MULTI_FINGER | 
            SYNAPTICS_MODE_GESTURE | SYNAPTICS_MODE_TAP_CLICK);
        
        uint8_t res = synaptics_query_byte(SYNAPTICS_QUERY_RESOLUTION);
        dev->res_x = (res & 0x0F) * 16 + 1;
        dev->res_y = ((res >> 4) & 0x0F) * 16 + 1;
        kprintf("[SYNAPTICS] Resolution: %dx%d\n", dev->res_x, dev->res_y);
        
        dev->min_x = 0;
        dev->max_x = dev->res_x * 100;
        dev->min_y = 0;
        dev->max_y = dev->res_y * 100;
        
        synaptics_write_cmd(SYNAPTICS_CMD_ENABLE);
        
        dev->present = true;
        kprintf("[SYNAPTICS] TouchPad initialized (absolute mode, multi-finger: %s)\n",
                g_synaptics.has_multi_finger ? "yes" : "no");
    } else {
        kprintf("[SYNAPTICS] Not a Synaptics device (ID: 0x%02X)\n", device_id);
    }
}

static void synaptics_poll(input_device_t *dev) {
    (void)dev;
    
    uint8_t status = inb(MOUSE_PORT_CMD);
    if (!(status & 0x01)) return;
    if (!(status & 0x20)) {
        inb(MOUSE_PORT_DATA);
        return;
    }
    
    uint8_t data = inb(MOUSE_PORT_DATA);
    
    if (synaptics_awaiting_ack) {
        if (data == SYNAPTICS_ACK) {
            synaptics_awaiting_ack = false;
        }
        return;
    }
    
    if (synaptics_cycle == 0) {
        if (!(data & 0x80)) return;
        synaptics_packet[0] = data;
        synaptics_cycle = 1;
        return;
    }
    
    if (synaptics_cycle < 6) {
        synaptics_packet[synaptics_cycle] = data;
        synaptics_cycle++;
    }
    
    if (synaptics_cycle == 6) {
        synaptics_cycle = 0;
        input_event_t event = {0};
        synaptics_decode_packet(synaptics_packet, &event);
    }
}

static void synaptics_enable(input_device_t *dev) {
    (void)dev;
    synaptics_write_cmd(SYNAPTICS_CMD_ENABLE);
}

static void synaptics_disable(input_device_t *dev) {
    (void)dev;
    synaptics_write_cmd(SYNAPTICS_CMD_DISABLE);
}

static bool synaptics_handle_irq(input_device_t *dev, uint8_t data) {
    (void)dev; (void)data;
    return false;
}

static void synaptics_set_resolution(input_device_t *dev, uint16_t x, uint16_t y) {
    (void)dev;
    synaptics_special_cmd(SYNAPTICS_SET_RESOLUTION, (x << 4) | y);
}

static void synaptics_set_scaling(input_device_t *dev, bool one_to_one) {
    (void)dev; (void)one_to_one;
}

static input_device_t g_synaptics_dev = {
    .type = INPUT_DEVICE_SYNAPTICS,
    .name = "Synaptics TouchPad",
    .present = false,
    .absolute_mode = true,
    .init = synaptics_init,
    .poll = synaptics_poll,
    .enable = synaptics_enable,
    .disable = synaptics_disable,
    .handle_irq = synaptics_handle_irq,
    .set_resolution = synaptics_set_resolution,
    .set_scaling = synaptics_set_scaling,
};

input_device_t *synaptics_create(void) {
    return &g_synaptics_dev;
}