#ifndef MOUSE_H
#define MOUSE_H

#include "../include/types.h"

typedef struct mouse_state {
    int16_t  x;
    int16_t  y;
    uint8_t  buttons;       /* bit 0=left, 1=right, 2=middle, 3=back, 4=forward */
    int8_t   scroll;        /* Vertical scroll wheel delta */
    int8_t   scroll_h;      /* Horizontal scroll wheel delta */
    bool     present;
    bool     scroll_supported;   /* True if mouse reports scroll wheel */
    bool     five_button;        /* True if mouse has 5 buttons */
} mouse_state_t;

/* Mouse event types */
typedef enum {
    MOUSE_EVENT_MOVE = 0,
    MOUSE_EVENT_BUTTON_DOWN,
    MOUSE_EVENT_BUTTON_UP,
    MOUSE_EVENT_SCROLL,
    MOUSE_EVENT_SCROLL_H
} mouse_event_type_t;

/* Mouse event structure */
typedef struct mouse_event {
    mouse_event_type_t type;
    int16_t x, y;
    uint8_t button;      /* Button that changed (for button events) */
    int8_t scroll_delta; /* Scroll amount (for scroll events) */
    uint32_t timestamp;  /* Timer tick when event occurred */
} mouse_event_t;

/* Initialize mouse driver */
void mouse_init(void);

/* Get current mouse state (polling) */
mouse_state_t mouse_get_state(void);

/* Get next mouse event from queue (non-blocking) */
bool mouse_get_event(mouse_event_t *event);

/* Set mouse position (for warping) */
void mouse_set_position(int16_t x, int16_t y);

/* Read the current pointer position without draining the event queue */
void mouse_get_position(int *x, int *y);

/* Set mouse sensitivity (1-10, default 5) */
void mouse_set_sensitivity(uint8_t sensitivity);
uint8_t mouse_get_sensitivity(void);

/* Enable/disable mouse acceleration */
void mouse_set_acceleration(bool enabled);
bool mouse_get_acceleration(void);

/* Get mouse button name */
const char *mouse_button_name(uint8_t button);

/* Inject a USB HID mouse report (button byte + signed deltas). dy is in USB
 * convention (positive = down); the driver flips it to the PS/2 convention. */
void mouse_input(uint8_t buttons, int8_t dx, int8_t dy, int8_t scroll);

#endif