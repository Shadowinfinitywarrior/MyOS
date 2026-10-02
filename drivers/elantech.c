#include "input.h"
#include "mouse.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../include/system.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define ELANTECH_ID_1 0x2C
#define ELANTECH_ID_2 0x48

#define ELANTECH_CMD_RESET           0xFF
#define ELANTECH_CMD_SET_SAMPLE_RATE 0xF3
#define ELANTECH_CMD_GET_DEVICE_ID   0xF2
#define ELANTECH_CMD_SET_RESOLUTION  0xE8
#define ELANTECH_CMD_ENABLE          0xF4
#define ELANTECH_CMD_DISABLE         0xF5
#define ELANTECH_CMD_SET_DEFAULTS    0xF6

#define ELANTECH_SPECIAL_CMD         0xE8
#define ELANTECH_QUERY_INFO          0x00
#define ELANTECH_SET_MODE            0x01
#define ELANTECH_QUERY_CAPS          0x02
#define ELANTECH_SET_RESOLUTION      0x03
#define ELANTECH_QUERY_HW            0x04

#define ELANTECH_MODE_RELATIVE       0x00
#define ELANTECH_MODE_ABSOLUTE       0x01
#define ELANTECH_MODE_MULTI_FINGER   0x02
#define ELANTECH_MODE_GESTURE        0x04
#define ELANTECH_MODE_TAP_CLICK      0x08

#define ELANTECH_ACK 0xFA
#define ELANTECH_NACK 0xFE
#define ELANTECH_ERROR 0xFC

typedef struct {
    uint8_t version;
    uint8_t hardware_version;
    uint8_t firmware_version;
    uint16_t min_x, max_x;
    uint16_t min_y, max_y;
    uint16_t min_pressure, max_pressure;
    uint16_t finger_width;
    bool has_multi_finger;
    bool has_gesture;
    bool has_palm_detect;
    bool absolute_mode;
    uint8_t mode_byte;
    uint8_t button_state;
    int16_t last_x, last_y;
    uint16_t last_z;
    uint8_t finger_count;
} elantech_state_t;

static elantech_state_t g_elantech = {0};
static volatile uint8_t elantech_packet[6];
static volatile uint8_t elantech_cycle = 0;
static volatile bool elantech_awaiting_ack = false;

static void elantech_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && (inb(MOUSE_PORT_CMD) & 0x02));
}

static void elantech_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(MOUSE_PORT_CMD) & 0x01));
}

static bool elantech_write_cmd(uint8_t cmd) {
    elantech_wait_output();
    outb(MOUSE_PORT_CMD, 0xD4);
    elantech_wait_output();
    outb(MOUSE_PORT_DATA, cmd);
    elantech_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == ELANTECH_ACK;
}

static bool elantech_write_cmd_arg(uint8_t cmd, uint8_t arg) {
    if (!elantech_write_cmd(cmd)) return false;
    elantech_wait_output();
    outb(MOUSE_PORT_DATA, arg);
    elantech_wait_input();
    uint8_t ack = inb(MOUSE_PORT_DATA);
    return ack == ELANTECH_ACK;
}

static uint8_t elantech_read_data(void) {
    elantech_wait_input();
    return inb(MOUSE_PORT_DATA);
}

static bool elantech_special_cmd(uint8_t subcmd, uint8_t arg) {
    if (!elantech_write_cmd(ELANTECH_SPECIAL_CMD)) return false;
    if (!elantech_write_cmd_arg(subcmd, arg)) return false;
    return true;
}

static uint8_t elantech_query_byte(uint8_t subcmd) {
    elantech_write_cmd(ELANTECH_SPECIAL_CMD);
    elantech_write_cmd_arg(subcmd, 0);
    return elantech_read_data();
}

static void elantech_decode_packet(volatile uint8_t *packet, input_event_t *event) {
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
    
    uint8_t changed = buttons ^ g_elantech.button_state;
    if (changed) {
        for (int b = 0; b < 3; b++) {
            if (changed & (1 << b)) {
                event->type = (buttons & (1 << b)) ? INPUT_EVENT_BUTTON_DOWN : INPUT_EVENT_BUTTON_UP;
                event->button = b;
                event->x = x;
                event->y = g_elantech.max_y - y;
                event->timestamp = timer_get_ticks();
                input_queue_event(event);
            }
        }
        g_elantech.button_state = buttons;
    }
    
    if (g_elantech.absolute_mode) {
        event->type = INPUT_EVENT_ABSOLUTE_MOVE;
        event->x = x;
        event->y = g_elantech.max_y - y;
        event->dx = x - g_elantech.last_x;
        event->dy = (g_elantech.max_y - y) - g_elantech.last_y;
        g_elantech.last_x = x;
        g_elantech.last_y = g_elantech.max_y - y;
    } else {
        int16_t dx = (int8_t)byte1;
        int16_t dy = (int8_t)byte3;
        event->type = INPUT_EVENT_MOVE;
        event->dx = dx;
        event->dy = -dy;
    }
    
    if (z != g_elantech.last_z) {
        if (z > 0 && g_elantech.last_z == 0) {
            event->type = INPUT_EVENT_TOUCH_DOWN;
            event->touches[0].x = x;
            event->touches[0].y = g_elantech.max_y - y;
            event->touches[0].pressure = z;
            event->touch_count = 1;
        } else if (z == 0 && g_elantech.last_z > 0) {
            event->type = INPUT_EVENT_TOUCH_UP;
            event->touch_count = 0;
        }
        g_elantech.last_z = z;
    }
    
    if (g_elantech.has_multi_finger) {
        uint8_t finger_info = elantech_query_byte(0x0A);
        g_elantech.finger_count = finger_info & 0x07;
        event->touch_count = g_elantech.finger_count;
        
        if (g_elantech.finger_count == 2) {
            event->type = INPUT_EVENT_GESTURE;
            event->gesture = GESTURE_SCROLL_TWO_FINGER;
        } else if (g_elantech.finger_count == 3) {
            event->type = INPUT_EVENT_GESTURE;
            event->gesture = GESTURE_THREE_FINGER_TAP;
        }
    }
    
    event->timestamp = timer_get_ticks();
    input_queue_event(event);
}

static void elantech_init(input_device_t *dev) {
    kprintf("[ELANTECH] Initializing Elantech TouchPad...\n");
    
    dev->min_x = 0;
    dev->max_x = 1023;
    dev->min_y = 0;
    dev->max_y = 767;
    
    elantech_write_cmd(ELANTECH_CMD_RESET);
    elantech_read_data();
    elantech_read_data();
    
    elantech_write_cmd(ELANTECH_CMD_SET_DEFAULTS);
    elantech_write_cmd(ELANTECH_CMD_SET_SAMPLE_RATE);
    elantech_write_cmd(100);
    
    elantech_write_cmd(ELANTECH_CMD_GET_DEVICE_ID);
    elantech_read_data();
    uint8_t device_id = elantech_read_data();
    kprintf("[ELANTECH] Device ID: 0x%02X\n", device_id);
    
    if (device_id == ELANTECH_ID_1 || device_id == ELANTECH_ID_2) {
        uint8_t hw_info = elantech_query_byte(ELANTECH_QUERY_HW);
        kprintf("[ELANTECH] Hardware info: 0x%02X\n", hw_info);
        
        g_elantech.hardware_version = (hw_info >> 4) & 0x0F;
        g_elantech.firmware_version = hw_info & 0x0F;
        
        g_elantech.has_multi_finger = true;
        g_elantech.has_gesture = true;
        g_elantech.has_palm_detect = true;
        
        dev->absolute_mode = true;
        g_elantech.absolute_mode = true;
        
        elantech_special_cmd(ELANTECH_SET_MODE, 
            ELANTECH_MODE_ABSOLUTE | ELANTECH_MODE_MULTI_FINGER | 
            ELANTECH_MODE_GESTURE | ELANTECH_MODE_TAP_CLICK);
        
        uint8_t res = elantech_query_byte(ELANTECH_SET_RESOLUTION);
        dev->res_x = (res & 0x0F) * 32 + 16;
        dev->res_y = ((res >> 4) & 0x0F) * 32 + 16;
        kprintf("[ELANTECH] Resolution: %dx%d\n", dev->res_x, dev->res_y);
        
        dev->min_x = 0;
        dev->max_x = dev->res_x * 100;
        dev->min_y = 0;
        dev->max_y = dev->res_y * 100;
        
        elantech_write_cmd(ELANTECH_CMD_ENABLE);
        
        dev->present = true;
        kprintf("[ELANTECH] TouchPad initialized (absolute mode, multi-finger: %s)\n",
                g_elantech.has_multi_finger ? "yes" : "no");
    } else {
        kprintf("[ELANTECH] Not an Elantech device (ID: 0x%02X)\n", device_id);
    }
}

static void elantech_poll(input_device_t *dev) {
    (void)dev;
    
    uint8_t status = inb(MOUSE_PORT_CMD);
    if (!(status & 0x01)) return;
    if (!(status & 0x20)) {
        inb(MOUSE_PORT_DATA);
        return;
    }
    
    uint8_t data = inb(MOUSE_PORT_DATA);
    
    if (elantech_awaiting_ack) {
        if (data == ELANTECH_ACK) {
            elantech_awaiting_ack = false;
        }
        return;
    }
    
    if (elantech_cycle == 0) {
        if (!(data & 0x80)) return;
        elantech_packet[0] = data;
        elantech_cycle = 1;
        return;
    }
    
    if (elantech_cycle < 6) {
        elantech_packet[elantech_cycle] = data;
        elantech_cycle++;
    }
    
    if (elantech_cycle == 6) {
        elantech_cycle = 0;
        input_event_t event = {0};
        elantech_decode_packet(elantech_packet, &event);
    }
}

static void elantech_enable(input_device_t *dev) {
    (void)dev;
    elantech_write_cmd(ELANTECH_CMD_ENABLE);
}

static void elantech_disable(input_device_t *dev) {
    (void)dev;
    elantech_write_cmd(ELANTECH_CMD_DISABLE);
}

static bool elantech_handle_irq(input_device_t *dev, uint8_t data) {
    (void)dev; (void)data;
    return false;
}

static void elantech_set_resolution(input_device_t *dev, uint16_t x, uint16_t y) {
    (void)dev;
    elantech_special_cmd(ELANTECH_SET_RESOLUTION, (x << 4) | y);
}

static void elantech_set_scaling(input_device_t *dev, bool one_to_one) {
    (void)dev; (void)one_to_one;
}

static input_device_t g_elantech_dev = {
    .type = INPUT_DEVICE_ELANTECH,
    .name = "Elantech TouchPad",
    .present = false,
    .absolute_mode = true,
    .init = elantech_init,
    .poll = elantech_poll,
    .enable = elantech_enable,
    .disable = elantech_disable,
    .handle_irq = elantech_handle_irq,
    .set_resolution = elantech_set_resolution,
    .set_scaling = elantech_set_scaling,
};

input_device_t *elantech_create(void) {
    return &g_elantech_dev;
}