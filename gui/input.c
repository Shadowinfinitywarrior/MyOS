#include "input.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../kernel/timer.h"

static gui_input_t in;
static uint8_t button_state;   /* sticky, so windows can test "is held" */

void input_push(const gui_event_t *e) {
    int next = (in.head + 1) % GUI_EVENT_QUEUE;
    if (next == in.tail) return;         /* full: drop, never overwrite */
    in.q[in.head] = *e;
    in.head = next;
    in.dirty = true;
}

void input_pump(void) {
    /* Adopt the driver's starting position on the first pump. Without this the
     * GUI cursor sits at (0,0) until the mouse happens to move, even though
     * the driver has been tracking it at the screen centre all along. */
    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        mouse_get_position(&in.px, &in.py);
    }

    mouse_event_t me;
    while (mouse_get_event(&me)) {
        gui_event_t e;
        e.x = me.x;
        e.y = me.y;
        e.dx = 0;
        e.dy = 0;
        e.keycode = 0;
        e.ascii = 0;
        e.modifiers = 0;
        e.repeat = false;
        e.timestamp = me.timestamp;
        e.scroll = 0;
        switch (me.type) {
            case MOUSE_EVENT_MOVE:
                e.type = EV_MOUSE_MOVE;
                e.dx = me.x - in.px;
                e.dy = me.y - in.py;
                break;
            case MOUSE_EVENT_BUTTON_DOWN:
                e.type = EV_MOUSE_DOWN;
                e.button = me.button;
                button_state |= (uint8_t)(1u << me.button);
                break;
            case MOUSE_EVENT_BUTTON_UP:
                e.type = EV_MOUSE_UP;
                e.button = me.button;
                button_state &= (uint8_t)~(1u << me.button);
                break;
            case MOUSE_EVENT_SCROLL:
                e.type = EV_MOUSE_SCROLL;
                e.scroll = me.scroll_delta;
                break;
            case MOUSE_EVENT_SCROLL_H:
                e.type = EV_MOUSE_SCROLL;
                break;
        }
        in.px = me.x;
        in.py = me.y;
        in.buttons = button_state;
        input_push(&e);
    }

    key_event_t ke;
    while (keyboard_get_event(&ke)) {
        gui_event_t e;
        e.x = in.px;
        e.y = in.py;
        e.dx = e.dy = 0;
        e.button = 0;
        e.scroll = 0;
        e.keycode = ke.keycode;
        e.ascii = ke.ascii;
        e.modifiers = ke.modifiers;
        e.repeat = (ke.type == KEY_EVENT_REPEAT);
        e.timestamp = timer_get_ticks();
        e.type = (ke.type == KEY_EVENT_UP) ? EV_KEY_UP : EV_KEY_DOWN;
        input_push(&e);
    }
}

bool input_poll(gui_event_t *out) {
    if (in.tail == in.head) { in.dirty = false; return false; }
    if (out) *out = in.q[in.tail];
    in.tail = (in.tail + 1) % GUI_EVENT_QUEUE;
    in.dirty = (in.tail != in.head);
    return true;
}

int input_pending(void) {
    return (in.head - in.tail + GUI_EVENT_QUEUE) % GUI_EVENT_QUEUE;
}

int input_mouse_x(void) { return in.px; }
int input_mouse_y(void) { return in.py; }
uint8_t input_mouse_buttons(void) { return in.buttons; }
bool input_button_down(uint8_t button) {
    if (button > 7) return false;
    return (button_state & (1u << button)) != 0;
}
