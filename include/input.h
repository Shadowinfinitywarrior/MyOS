#ifndef INPUT_H
#define INPUT_H

#include "../include/types.h"

typedef enum {
    INPUT_DEVICE_NONE = 0,
    INPUT_DEVICE_PS2_MOUSE,
    INPUT_DEVICE_SYNAPTICS,
    INPUT_DEVICE_ALPS,
    INPUT_DEVICE_ELANTECH,
    INPUT_DEVICE_USB_MOUSE,
    INPUT_DEVICE_USB_TOUCHSCREEN,
    INPUT_DEVICE_TABLET,
} input_device_type_t;

typedef enum {
    INPUT_EVENT_MOVE = 0,
    INPUT_EVENT_BUTTON_DOWN,
    INPUT_EVENT_BUTTON_UP,
    INPUT_EVENT_SCROLL,
    INPUT_EVENT_SCROLL_H,
    INPUT_EVENT_ABSOLUTE_MOVE,
    INPUT_EVENT_TOUCH_DOWN,
    INPUT_EVENT_TOUCH_UP,
    INPUT_EVENT_TOUCH_MOVE,
    INPUT_EVENT_GESTURE,
} input_event_type_t;

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_SCROLL_TWO_FINGER,
    GESTURE_PINCH,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_THREE_FINGER_TAP,
} gesture_type_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    uint8_t pressure;
    uint8_t finger_count;
    uint16_t major_axis;
    uint16_t minor_axis;
    int16_t orientation;
} touch_point_t;

#define MAX_TOUCH_POINTS 5

typedef struct {
    input_event_type_t type;
    int16_t x, y;
    int16_t dx, dy;
    uint8_t button;
    int8_t scroll_delta;
    touch_point_t touches[MAX_TOUCH_POINTS];
    uint8_t touch_count;
    gesture_type_t gesture;
    uint32_t timestamp;
} input_event_t;

typedef struct input_device input_device_t;

struct input_device {
    input_device_type_t type;
    const char *name;
    bool present;
    bool absolute_mode;
    int16_t min_x, max_x;
    int16_t min_y, max_y;
    int16_t min_pressure, max_pressure;
    int16_t res_x, res_y;
    
    void (*init)(input_device_t *dev);
    void (*poll)(input_device_t *dev);
    void (*enable)(input_device_t *dev);
    void (*disable)(input_device_t *dev);
    bool (*handle_irq)(input_device_t *dev, uint8_t data);
    void (*set_resolution)(input_device_t *dev, uint16_t x, uint16_t y);
    void (*set_scaling)(input_device_t *dev, bool one_to_one);
    
    void *private_data;
    input_device_t *next;
};

typedef struct {
    input_device_t *devices;
    input_event_t event_queue[256];
    volatile int queue_head;
    volatile int queue_tail;
    int16_t cursor_x;
    int16_t cursor_y;
    uint8_t sensitivity;
    bool acceleration;
    bool tap_to_click;
    bool natural_scroll;
} input_system_t;

void input_init(void);
void input_register_device(input_device_t *dev);
void input_unregister_device(input_device_t *dev);
input_device_t *input_find_device(input_device_type_t type);
void input_queue_event(const input_event_t *event);
bool input_get_event(input_event_t *event);
void input_set_cursor(int16_t x, int16_t y);
void input_get_cursor(int16_t *x, int16_t *y);
void input_set_sensitivity(uint8_t s);
void input_set_acceleration(bool enable);
void input_set_tap_to_click(bool enable);
void input_set_natural_scroll(bool enable);
input_system_t *input_get_system(void);

input_device_t *synaptics_create(void);
input_device_t *alps_create(void);
input_device_t *elantech_create(void);
input_device_t *ps2_mouse_create(void);
input_device_t *usb_mouse_create(void);
input_device_t *usb_touchscreen_create(void);

void input_detect_all(void);

#endif