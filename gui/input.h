#ifndef GUI_INPUT_H
#define GUI_INPUT_H

#include "rect.h"
#include "../include/types.h"

/* Unified input for the GUI.
 *
 * Two sources feed it: the PS/2 mouse driver's event ring (already integrated
 * to absolute screen coordinates) and the keyboard driver's event ring. The
 * pump drains both into one queue that the window manager and its windows
 * consume, so window code never talks to a driver directly.
 */

typedef enum {
    EV_NONE = 0,
    EV_MOUSE_MOVE,
    EV_MOUSE_DOWN,
    EV_MOUSE_UP,
    EV_MOUSE_SCROLL,
    EV_KEY_DOWN,
    EV_KEY_UP
} ev_type_t;

typedef struct gui_event {
    ev_type_t type;
    int       x, y;        /* pointer position in screen coords     */
    int       dx, dy;      /* pointer delta since the last event     */
    uint8_t   button;      /* 0=left 1=right 2=middle                */
    int8_t    scroll;
    uint16_t  keycode;     /* USB-HID-style usage id (see keyboard.h) */
    char      ascii;       /* translated character, 0 if none        */
    uint8_t   modifiers;   /* KMOD_* from keyboard.h                 */
    bool      repeat;
    uint32_t  timestamp;
} gui_event_t;

#define GUI_EVENT_QUEUE 128

typedef struct {
    gui_event_t q[GUI_EVENT_QUEUE];
    int         head, tail;
    /* Latest pointer state, for hit-testing without draining the queue. */
    int         px, py;
    uint8_t     buttons;
    /* Set when the queue is nonempty; the WM loop spins on this. */
    bool        dirty;
} gui_input_t;

void      input_pump(void);                 /* drain drivers into the queue */
bool      input_poll(gui_event_t *out);     /* pop one event, non-blocking  */
int       input_pending(void);
void      input_push(const gui_event_t *e); /* inject (used by the GUI shell) */

int       input_mouse_x(void);
int       input_mouse_y(void);
uint8_t   input_mouse_buttons(void);

/* Button state of a specific button, tracked across the whole session. */
bool      input_button_down(uint8_t button);

/* Special keycodes (USB HID usage ids) the window manager and terminal care
 * about. Values match drivers/keyboard.h. */
#define GUIKEY_ENTER      0x0028
#define GUIKEY_ESCAPE     0x0029
#define GUIKEY_BACKSPACE  0x002A
#define GUIKEY_TAB        0x002B
#define GUIKEY_SPACE      0x002C
#define GUIKEY_UP         0x0052
#define GUIKEY_DOWN       0x0051
#define GUIKEY_RIGHT      0x0050
#define GUIKEY_LEFT       0x004F
#define GUIKEY_HOME       0x004A
#define GUIKEY_END        0x004B
#define GUIKEY_PGUP       0x004E
#define GUIKEY_PGDN       0x004D
#define GUIKEY_DELETE     0x004C
#define GUIKEY_1          0x001E
#define GUIKEY_2          0x001F
#define GUIKEY_3          0x0020
#define GUIKEY_4          0x0021
#define GUIKEY_5          0x0022
#define GUIKEY_6          0x0023
#define GUIKEY_7          0x0024
#define GUIKEY_8          0x0025
#define GUIKEY_9          0x0026
#define GUIKEY_0          0x0027
#define GUIKEY_D          0x0007
#define GUIKEY_ALT        0x00E2
#define GUIKEY_ALT_GR     0x00E6
#define GUIKEY_SUPER      0x00E3
#define GUIKEY_SUPER_R    0x00E7

#endif
