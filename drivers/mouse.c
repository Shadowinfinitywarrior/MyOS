#include "mouse.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../include/system.h"
#include "../lib/printf.h"
#include "../drivers/framebuffer.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MOUSE_PORT_DATA   0x60
#define MOUSE_PORT_CMD    0x64

/* Mouse commands */
#define MOUSE_CMD_RESET           0xFF
#define MOUSE_CMD_SET_SAMPLE_RATE 0xF3
#define MOUSE_CMD_GET_DEVICE_ID   0xF2
#define MOUSE_CMD_SET_SCALING_1_1 0xE6
#define MOUSE_CMD_SET_SCALING_2_1 0xE7
#define MOUSE_CMD_SET_RESOLUTION  0xE8
#define MOUSE_CMD_SET_STREAM_MODE 0xF4
#define MOUSE_CMD_DISABLE         0xF5
#define MOUSE_CMD_ENABLE          0xF4
#define MOUSE_CMD_SET_DEFAULTS    0xF6
#define MOUSE_CMD_READ_DATA       0xEB

/* Mouse status responses */
#define MOUSE_ACK                 0xFA
#define MOUSE_NACK                0xFE
#define MOUSE_ERROR               0xFC

/* Mouse IDs */
#define MOUSE_ID_STANDARD         0x00
#define MOUSE_ID_INTELLIMOUSE     0x03  /* Scroll wheel */
#define MOUSE_ID_INTELLIMOUSE_EX  0x04  /* 5 buttons */

/* Event queue */
#define MOUSE_QUEUE_SIZE          128
static volatile mouse_event_t mouse_queue[MOUSE_QUEUE_SIZE];
static volatile int mouse_queue_head = 0;
static volatile int mouse_queue_tail = 0;

/* Mouse state */
static volatile mouse_state_t mouse = { 
    .x = 512, .y = 384, 
    .buttons = 0, 
    .scroll = 0, 
    .scroll_h = 0, 
    .present = false,
    .scroll_supported = false,
    .five_button = false
};

/* Parsing state */
static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[4];  /* Support up to 4 bytes (5-button) */
static bool expecting_4th_byte = false;
static bool mouse_awaiting_ack = false;

/* Mouse settings */
static uint8_t mouse_sensitivity = 5;  /* 1-10 */
static bool mouse_acceleration = true;
static int16_t last_dx = 0, last_dy = 0;

/* Previous button state for event detection */
static uint8_t prev_buttons = 0;

/* Queue a mouse event */
static void mouse_queue_event(mouse_event_type_t type, int16_t x, int16_t y, uint8_t button, int8_t scroll_delta) {
    int next = (mouse_queue_head + 1) % MOUSE_QUEUE_SIZE;
    if (next != mouse_queue_tail) {
        mouse_queue[mouse_queue_head].type = type;
        mouse_queue[mouse_queue_head].x = x;
        mouse_queue[mouse_queue_head].y = y;
        mouse_queue[mouse_queue_head].button = button;
        mouse_queue[mouse_queue_head].scroll_delta = scroll_delta;
        mouse_queue[mouse_queue_head].timestamp = timer_get_ticks();
        mouse_queue_head = next;
    }
}

/* Wait for mouse controller input buffer empty */
static void mouse_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && (inb(MOUSE_PORT_CMD) & 0x02));
}

/* Wait for mouse controller output buffer full */
static void mouse_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(MOUSE_PORT_CMD) & 0x01));
}

/* Write command to mouse */
static void mouse_write(uint8_t data) {
    mouse_wait_output();
    outb(MOUSE_PORT_CMD, 0xD4);

    mouse_wait_output();
    outb(MOUSE_PORT_DATA, data);
}

/* Read response from mouse */
static uint8_t mouse_read(void) {
    /* Disable IRQ handler from consuming our ACK */
    bool old_awaiting_ack = mouse_awaiting_ack;
    mouse_awaiting_ack = false;
    
    mouse_wait_input();
    uint8_t data = inb(MOUSE_PORT_DATA);
    
    /* Re-enable ACK waiting if we were waiting before */
    mouse_awaiting_ack = old_awaiting_ack;
    return data;
}

/* Get current screen resolution from framebuffer */
static void get_screen_bounds(int *max_x, int *max_y) {
    fb_info_t *fb = fb_get_info();
    if (fb && fb->width > 0 && fb->height > 0) {
        *max_x = fb->width - 1;
        *max_y = fb->height - 1;
    } else {
        *max_x = 1023;
        *max_y = 767;
    }
}

/* Apply sensitivity and acceleration to mouse delta */
static int apply_sensitivity(int delta) {
    if (delta == 0) return 0;
    
    int abs_delta = delta < 0 ? -delta : delta;
    int sign = delta < 0 ? -1 : 1;
    
    /* Apply sensitivity (1-10, default 5) */
    abs_delta = (abs_delta * mouse_sensitivity) / 5;

    /* Acceleration exists to make fine positioning easier, so it boosts only
     * the very smallest movement. Applying it to longer ones is actively
     * harmful: a single 60px packet became 60 + (58*58)/4 = 901px, which
     * slammed the pointer into the screen edge. Doubling every packet in the
     * 2..8 range was wrong too - QEMU splits one long move into many small
     * packets, so the boost accumulated and the pointer overshot its target by
     * roughly 10%. Emitting 2 for a 1px step keeps the feature without moving
     * the goalposts under the pointer. */
    if (mouse_acceleration && abs_delta == 1) {
        abs_delta = 2;
    }

    return abs_delta * sign;
}

/* Shared post-decode pipeline: apply sensitivity/acceleration, update state,
 * detect button/scroll changes and queue events. Used by the PS/2 IRQ path
 * (mouse_callback) and by the USB HID path (mouse_input). */
static void mouse_process_packet(uint8_t byte0, int dx, int dy, int8_t scroll, int8_t scroll_h) {
    /* Apply sensitivity and acceleration */
    dx = apply_sensitivity(dx);
    dy = apply_sensitivity(dy);

    /* Update position */
    mouse.x += dx;
    mouse.y -= dy;   /* Y is inverted */

    /* Clamp to screen bounds */
    int max_x, max_y;
    get_screen_bounds(&max_x, &max_y);
    
    if (mouse.x < 0) mouse.x = 0;
    if (mouse.x > max_x) mouse.x = max_x;
    if (mouse.y < 0) mouse.y = 0;
    if (mouse.y > max_y) mouse.y = max_y;

    /* Update buttons */
    uint8_t new_buttons = byte0 & 0x07;

    /* Detect button changes and queue events */
    uint8_t changed = new_buttons ^ prev_buttons;
    if (changed) {
        for (int b = 0; b < 3; b++) {
            if (changed & (1 << b)) {
                if (new_buttons & (1 << b)) {
                    mouse_queue_event(MOUSE_EVENT_BUTTON_DOWN, mouse.x, mouse.y, b, 0);
                } else {
                    mouse_queue_event(MOUSE_EVENT_BUTTON_UP, mouse.x, mouse.y, b, 0);
                }
            }
        }
        prev_buttons = new_buttons;
    }
    
    mouse.buttons = new_buttons;

    /* Handle vertical scroll */
    if (scroll != 0) {
        mouse.scroll = scroll;
        mouse_queue_event(MOUSE_EVENT_SCROLL, mouse.x, mouse.y, 0, scroll);
    } else {
        mouse.scroll = 0;
    }
    
    /* Handle horizontal scroll */
    if (scroll_h != 0) {
        mouse.scroll_h = scroll_h;
        mouse_queue_event(MOUSE_EVENT_SCROLL_H, mouse.x, mouse.y, 0, scroll_h);
    } else {
        mouse.scroll_h = 0;
    }

    /* Queue move event if position changed */
    if (dx != 0 || dy != 0) {
        mouse_queue_event(MOUSE_EVENT_MOVE, mouse.x, mouse.y, 0, 0);
        last_dx = dx;
        last_dy = dy;
    }
}

/* USB HID mouse packet entry point. The USB report delivers dy positive when
 * the pointer moves DOWN the screen; the shared pipeline expects PS/2-style
 * positive dy == up, so the value is negated here. */
void mouse_input(uint8_t buttons, int8_t dx, int8_t dy, int8_t scroll) {
    mouse_process_packet(buttons, dx, -(int)dy, scroll, 0);
}

/* Mouse interrupt handler */
static void mouse_callback(registers_t *regs) {
    (void)regs;
    /* IRQ entry hook for GUI input ring */

    uint8_t status = inb(MOUSE_PORT_CMD);
    /* Accept mouse data if either bit 4 (aux output buffer) or bit 5 (mouse data) is set */
    if (!(status & 0x30)) {
        return;   /* Not mouse data */
    }

    int8_t data = (int8_t)inb(MOUSE_PORT_DATA);

    /* The i8042 posts its own responses on the same data port, and they can
     * arrive at any time - including long after the command that provoked
     * them, when mouse_awaiting_ack has already been cleared. Every one of
     * these has bit 3 set, so a naive "bit 3 set" sync test happily accepts
     * them as packet headers and shifts the whole packet stream by one byte
     * from then on, which shows up as wild garbage pointer motion. Filter the
     * reserved responses unconditionally instead of relying on the ack flag. */
    uint8_t raw = (uint8_t)data;
    if (raw == MOUSE_ACK || raw == MOUSE_ERROR || raw == MOUSE_NACK ||
        raw == MOUSE_CMD_ENABLE || raw == MOUSE_CMD_DISABLE ||
        raw == MOUSE_CMD_SET_DEFAULTS) {
        mouse_awaiting_ack = false;
        return;
    }

    if (mouse_cycle == 0) {
        /* First byte - must have bit 3 set for sync */
        if (!(data & 0x08)) return;
        mouse_bytes[0] = data;
        mouse_cycle = 1;

        /* Check if we're expecting 4-byte packets (scroll wheel or 5-button mouse) */
        expecting_4th_byte = mouse.scroll_supported || mouse.five_button;
        return;
    }
    
    if (mouse_cycle == 1) {
        mouse_bytes[1] = data;
        mouse_cycle = 2;
        return;
    }
    
    if (mouse_cycle == 2) {
        mouse_bytes[2] = data;
        
        if (expecting_4th_byte) {
            mouse_cycle = 3;
            return;
        }
        
        mouse_cycle = 0;
    }
    
    if (mouse_cycle == 3) {
        /* In a wheel packet byte 3 only carries a 4-bit wheel delta, so any
         * value with bits above the low nibble means the stream slipped and we
         * are looking at the wrong byte. Drop it and resync on the next one
         * that looks like a header, instead of decoding a corrupted packet. */
        if (mouse.scroll_supported && !mouse.five_button && (data & 0xF0)) {
            mouse_cycle = 0;
            return;
        }
        mouse_bytes[3] = data;
        mouse_cycle = 0;
    }

    /* Decode standard 3-byte packet */
    uint8_t byte0 = mouse_bytes[0];
    int dx = (int)mouse_bytes[1];
    int dy = (int)mouse_bytes[2];
    int8_t scroll = 0;
    int8_t scroll_h = 0;

    /* Sign from byte0, magnitude is unsigned */
    if (byte0 & 0x10) dx = -dx;
    if (byte0 & 0x20) dy = -dy;

    /* Handle scroll wheel (4th byte in IntelliMouse) */
    if (mouse.scroll_supported && !mouse.five_button) {
        scroll = (int8_t)mouse_bytes[3];
    }
    
    /* Handle horizontal scroll and 4th/5th buttons (5th byte in IntelliMouse Explorer) */
    if (mouse.five_button) {
        scroll = (int8_t)mouse_bytes[3];
        /* Byte 4 would contain horizontal scroll and buttons 4/5 */
        /* For now we just track the presence */
    }

    /* Shared pipeline: sensitivity, position, clamping, button/scroll events */
    mouse_process_packet(byte0, dx, dy, scroll, scroll_h);
}

void mouse_init(void) {
    kprintf("[MOUSE] Initializing PS/2 mouse...\n");
    /* Enable auxiliary mouse device */
    mouse_wait_output();
    outb(MOUSE_PORT_CMD, 0xA8);

    /* Enable interrupts */
    mouse_wait_output();
    outb(MOUSE_PORT_CMD, 0x20);
    mouse_wait_input();
    uint8_t status = inb(MOUSE_PORT_DATA) | 2;  /* Enable IRQ12 */
    mouse_wait_output();
    outb(MOUSE_PORT_CMD, 0x60);
    mouse_wait_output();
    outb(MOUSE_PORT_DATA, status);

    /* Reset mouse */
    mouse_write(MOUSE_CMD_RESET);
    uint8_t ack = mouse_read();

    kprintf("[MOUSE] Reset ACK: 0x%02X\n", ack);
    
    if (ack != MOUSE_ACK) {
        kprintf("[MOUSE] Failed to get ACK after reset\n");
        return;
    }
    mouse_read();

    mouse_read();


    /* Set defaults */
    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    mouse_read();


    /* Try to detect IntelliMouse (scroll wheel) */
    /* Set sample rate to 200, 100, 80 to enable scroll wheel */
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read();
    mouse_write(200);
    mouse_read();
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read();
    mouse_write(100);
    mouse_read();
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read();
    mouse_write(80);
    mouse_read();

    /* Get device ID */
    mouse_write(MOUSE_CMD_GET_DEVICE_ID);
    uint8_t response = mouse_read();
    if (response == MOUSE_ACK) {
        uint8_t device_id = mouse_read();
        if (device_id == MOUSE_ID_INTELLIMOUSE) {
            mouse.scroll_supported = true;
            kprintf("[MOUSE] Scroll wheel detected (IntelliMouse)\n");
        } else if (device_id == MOUSE_ID_INTELLIMOUSE_EX) {
            mouse.scroll_supported = true;
            mouse.five_button = true;
            kprintf("[MOUSE] 5-button mouse detected (IntelliMouse Explorer)\n");
        } else {
            kprintf("[MOUSE] Standard PS/2 mouse (ID: 0x%02X)\n", device_id);
        }
    }

    /* Set sample rate to 100 Hz */
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read();
    mouse_write(100);
    mouse_read();

    /* Set resolution to 8 counts/mm (value 2) */
    mouse_write(MOUSE_CMD_SET_RESOLUTION);
    mouse_read();

    mouse_write(2);
    mouse_read();


    /* Set scaling 1:1 */
    mouse_write(MOUSE_CMD_SET_SCALING_1_1);
    mouse_read();


    /* Enable data reporting */
    mouse_write(MOUSE_CMD_SET_STREAM_MODE);
    mouse_read();


    /* Flush any pending data */
    for (int i = 0; i < 10; i++) {
        if (inb(MOUSE_PORT_CMD) & 0x01) {
            inb(MOUSE_PORT_DATA);
        }
    }

    /* Register IRQ12 handler */
    isr_register_handler(IRQ12, mouse_callback);
    pic_clear_mask(12);

    mouse.present = true;
    mouse.x = 512;
    mouse.y = 384;
    prev_buttons = 0;
    
    kprintf("[MOUSE] Enhanced PS/2 mouse initialized (sensitivity: %d, accel: %s)\n", 
            (int)mouse_sensitivity, mouse_acceleration ? "on" : "off");
}

mouse_state_t mouse_get_state(void) {
    /* Return copy of current state */
    return mouse;
}

bool mouse_get_event(mouse_event_t *event) {
    if (mouse_queue_head == mouse_queue_tail) return false;
    
    *event = mouse_queue[mouse_queue_tail];
    mouse_queue_tail = (mouse_queue_tail + 1) % MOUSE_QUEUE_SIZE;
    return true;
}

void mouse_get_position(int *x, int *y) {
    if (x) *x = mouse.x;
    if (y) *y = mouse.y;
}

void mouse_set_position(int16_t x, int16_t y) {
    int max_x, max_y;
    get_screen_bounds(&max_x, &max_y);
    
    if (x < 0) x = 0;
    if (x > max_x) x = max_x;
    if (y < 0) y = 0;
    if (y > max_y) y = max_y;
    
    mouse.x = x;
    mouse.y = y;
}

void mouse_set_sensitivity(uint8_t sensitivity) {
    if (sensitivity < 1) sensitivity = 1;
    if (sensitivity > 10) sensitivity = 10;
    mouse_sensitivity = sensitivity;
}

void mouse_set_acceleration(bool enabled) {
    mouse_acceleration = enabled;
}

const char *mouse_button_name(uint8_t button) {
    switch (button) {
        case 0: return "Left";
        case 1: return "Right";
        case 2: return "Middle";
        case 3: return "Back";
        case 4: return "Forward";
        default: return "Unknown";
    }
}