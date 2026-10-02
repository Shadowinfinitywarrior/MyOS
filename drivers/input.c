#include "input.h"
#include "mouse.h"
#include "keyboard.h"
#include "usb_hid.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../include/system.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static input_system_t g_input = {0};

void input_init(void) {
    kprintf("[INPUT] Initializing input subsystem...\n");
    
    g_input.cursor_x = 512;
    g_input.cursor_y = 384;
    g_input.sensitivity = 5;
    g_input.acceleration = true;
    g_input.tap_to_click = true;
    g_input.natural_scroll = false;
    g_input.queue_head = 0;
    g_input.queue_tail = 0;
    g_input.devices = NULL;
    
    input_detect_all();
    
    kprintf("[INPUT] Input subsystem initialized\n");
}

void input_register_device(input_device_t *dev) {
    if (!dev) return;
    
    dev->next = g_input.devices;
    g_input.devices = dev;
    
    if (dev->init) dev->init(dev);
    if (dev->enable) dev->enable(dev);
    
    kprintf("[INPUT] Registered device: %s (type %d)\n", dev->name, dev->type);
}

void input_unregister_device(input_device_t *dev) {
    if (!dev) return;
    
    if (g_input.devices == dev) {
        g_input.devices = dev->next;
    } else {
        input_device_t *cur = g_input.devices;
        while (cur && cur->next != dev) cur = cur->next;
        if (cur) cur->next = dev->next;
    }
    
    if (dev->disable) dev->disable(dev);
    
    kprintf("[INPUT] Unregistered device: %s\n", dev->name);
}

input_device_t *input_find_device(input_device_type_t type) {
    input_device_t *dev = g_input.devices;
    while (dev) {
        if (dev->type == type) return dev;
        dev = dev->next;
    }
    return NULL;
}

void input_queue_event(const input_event_t *event) {
    int next = (g_input.queue_head + 1) % 256;
    if (next != g_input.queue_tail) {
        g_input.event_queue[g_input.queue_head] = *event;
        g_input.queue_head = next;
    }
}

bool input_get_event(input_event_t *event) {
    if (g_input.queue_head == g_input.queue_tail) return false;
    
    *event = g_input.event_queue[g_input.queue_tail];
    g_input.queue_tail = (g_input.queue_tail + 1) % 256;
    return true;
}

void input_set_cursor(int16_t x, int16_t y) {
    input_device_t *dev = g_input.devices;
    int max_x = 1023, max_y = 767;
    
    while (dev) {
        if (dev->absolute_mode) {
            max_x = dev->max_x;
            max_y = dev->max_y;
            break;
        }
        dev = dev->next;
    }
    
    if (x < 0) x = 0;
    if (x > max_x) x = max_x;
    if (y < 0) y = 0;
    if (y > max_y) y = max_y;
    
    g_input.cursor_x = x;
    g_input.cursor_y = y;
}

void input_get_cursor(int16_t *x, int16_t *y) {
    *x = g_input.cursor_x;
    *y = g_input.cursor_y;
}

void input_set_sensitivity(uint8_t s) {
    if (s < 1) s = 1;
    if (s > 10) s = 10;
    g_input.sensitivity = s;
}

void input_set_acceleration(bool enable) {
    g_input.acceleration = enable;
}

void input_set_tap_to_click(bool enable) {
    g_input.tap_to_click = enable;
}

void input_set_natural_scroll(bool enable) {
    g_input.natural_scroll = enable;
}

input_system_t *input_get_system(void) {
    return &g_input;
}

void input_poll_all(void) {
    input_device_t *dev = g_input.devices;
    while (dev) {
        if (dev->present && dev->poll) {
            dev->poll(dev);
        }
        dev = dev->next;
    }
}

static void ps2_mouse_init(input_device_t *dev) {
    (void)dev;
    mouse_init();
}

static void ps2_mouse_poll(input_device_t *dev) {
    (void)dev;
    mouse_poll();
}

static void ps2_mouse_enable(input_device_t *dev) {
    (void)dev;
}

static void ps2_mouse_disable(input_device_t *dev) {
    (void)dev;
}

static bool ps2_mouse_handle_irq(input_device_t *dev, uint8_t data) {
    (void)dev; (void)data;
    return false;
}

static void ps2_mouse_set_resolution(input_device_t *dev, uint16_t x, uint16_t y) {
    (void)dev; (void)x; (void)y;
}

static void ps2_mouse_set_scaling(input_device_t *dev, bool one_to_one) {
    (void)dev; (void)one_to_one;
}

static input_device_t g_ps2_mouse = {
    .type = INPUT_DEVICE_PS2_MOUSE,
    .name = "PS/2 Mouse",
    .present = false,
    .absolute_mode = false,
    .init = ps2_mouse_init,
    .poll = ps2_mouse_poll,
    .enable = ps2_mouse_enable,
    .disable = ps2_mouse_disable,
    .handle_irq = ps2_mouse_handle_irq,
    .set_resolution = ps2_mouse_set_resolution,
    .set_scaling = ps2_mouse_set_scaling,
};

static void usb_mouse_init(input_device_t *dev) {
    (void)dev;
}

static void usb_mouse_poll(input_device_t *dev) {
    (void)dev;
}

static void usb_mouse_enable(input_device_t *dev) {
    (void)dev;
}

static void usb_mouse_disable(input_device_t *dev) {
    (void)dev;
}

static bool usb_mouse_handle_irq(input_device_t *dev, uint8_t data) {
    (void)dev; (void)data;
    return false;
}

static void usb_mouse_set_resolution(input_device_t *dev, uint16_t x, uint16_t y) {
    (void)dev; (void)x; (void)y;
}

static void usb_mouse_set_scaling(input_device_t *dev, bool one_to_one) {
    (void)dev; (void)one_to_one;
}

static input_device_t g_usb_mouse = {
    .type = INPUT_DEVICE_USB_MOUSE,
    .name = "USB Mouse",
    .present = false,
    .absolute_mode = false,
    .init = usb_mouse_init,
    .poll = usb_mouse_poll,
    .enable = usb_mouse_enable,
    .disable = usb_mouse_disable,
    .handle_irq = usb_mouse_handle_irq,
    .set_resolution = usb_mouse_set_resolution,
    .set_scaling = usb_mouse_set_scaling,
};

input_device_t *ps2_mouse_create(void) {
    return &g_ps2_mouse;
}

input_device_t *usb_mouse_create(void) {
    return &g_usb_mouse;
}

void input_detect_all(void) {
    input_device_t *ps2 = ps2_mouse_create();
    if (ps2 && ps2->init) {
        ps2->present = true;
        input_register_device(ps2);
    }
    
    input_device_t *synaptics = synaptics_create();
    if (synaptics && synaptics->init) {
        input_register_device(synaptics);
    }
    
    input_device_t *alps = alps_create();
    if (alps && alps->init) {
        input_register_device(alps);
    }
    
    input_device_t *elantech = elantech_create();
    if (elantech && elantech->init) {
        input_register_device(elantech);
    }
    
    input_device_t *usb = usb_mouse_create();
    if (usb && usb->init) {
        usb->present = true;
        input_register_device(usb);
    }
}

void input_event_from_mouse(const mouse_event_t *me) {
    input_event_t ie = {0};
    ie.timestamp = me->timestamp;
    
    switch (me->type) {
        case MOUSE_EVENT_MOVE:
            ie.type = INPUT_EVENT_MOVE;
            ie.x = me->x;
            ie.y = me->y;
            {
                static int16_t last_x = 512, last_y = 384;
                ie.dx = me->x - last_x;
                ie.dy = me->y - last_y;
                last_x = me->x;
                last_y = me->y;
            }
            /* Keep the unified input cursor in sync with the absolute
             * pointer position so the compositor can render the cursor at
             * the real location (input_set_cursor also clamps to bounds). */
            input_set_cursor(me->x, me->y);
            break;
        case MOUSE_EVENT_BUTTON_DOWN:
            ie.type = INPUT_EVENT_BUTTON_DOWN;
            ie.x = me->x;
            ie.y = me->y;
            ie.button = me->button;
            break;
        case MOUSE_EVENT_BUTTON_UP:
            ie.type = INPUT_EVENT_BUTTON_UP;
            ie.x = me->x;
            ie.y = me->y;
            ie.button = me->button;
            break;
        case MOUSE_EVENT_SCROLL:
            ie.type = INPUT_EVENT_SCROLL;
            ie.x = me->x;
            ie.y = me->y;
            ie.scroll_delta = g_input.natural_scroll ? -me->scroll_delta : me->scroll_delta;
            break;
        case MOUSE_EVENT_SCROLL_H:
            ie.type = INPUT_EVENT_SCROLL_H;
            ie.x = me->x;
            ie.y = me->y;
            ie.scroll_delta = me->scroll_delta;
            break;
    }
    
    input_queue_event(&ie);
}

void input_process_mouse_events(void) {
    mouse_event_t me;
    while (mouse_get_event(&me)) {
        input_event_from_mouse(&me);
    }
}