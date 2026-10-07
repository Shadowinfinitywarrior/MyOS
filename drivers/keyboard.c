#include "keyboard.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../include/types.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#include "../include/rust_gui.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT    0x64

/* Keyboard controller commands */
#define KBD_CMD_SET_LEDS        0xED
#define KBD_CMD_SET_REPEAT      0xF3
#define KBD_CMD_ENABLE          0xF4
#define KBD_CMD_DISABLE         0xF5
#define KBD_CMD_RESET           0xFF

/* Keyboard status bits */
#define KBD_STATUS_OBF          0x01  /* Output buffer full */
#define KBD_STATUS_IBF          0x02  /* Input buffer full */

/* Keyboard LED bits */
#define KBD_LED_SCROLL_LOCK     0x01
#define KBD_LED_NUM_LOCK        0x02
#define KBD_LED_CAPS_LOCK       0x04

/* Event queue */
#define KEYBOARD_QUEUE_SIZE     256
static volatile key_event_t keyboard_queue[KEYBOARD_QUEUE_SIZE];
static volatile int keyboard_queue_head = 0;
static volatile int keyboard_queue_tail = 0;

/* Modifier state */
static volatile uint8_t keyboard_modifiers = 0;

/* Key state tracking for repeat */
#define MAX_KEYS_TRACKED        128
static volatile bool key_pressed[MAX_KEYS_TRACKED];
static volatile uint32_t key_press_time[MAX_KEYS_TRACKED];
static volatile bool key_repeat_sent[MAX_KEYS_TRACKED];

/* Repeat settings */
static uint32_t repeat_delay_ms = KEY_REPEAT_DELAY_MS;
static uint32_t repeat_rate_ms = KEY_REPEAT_RATE_MS;
static bool repeat_enabled = true;

/* Extended scancode flag */
static volatile bool extended_scancode = false;

/* Pause key handling (E1 1D 45 E1 9D C5) */
static volatile int pause_sequence = 0;

/* Scancode to keycode mapping (set 1, non-extended) */
static const uint16_t scancode_to_keycode[128] = {
    KEY_UNKNOWN,        /* 0x00 */
    KEY_ESCAPE,         /* 0x01 */
    KEY_1,              /* 0x02 */
    KEY_2,              /* 0x03 */
    KEY_3,              /* 0x04 */
    KEY_4,              /* 0x05 */
    KEY_5,              /* 0x06 */
    KEY_6,              /* 0x07 */
    KEY_7,              /* 0x08 */
    KEY_8,              /* 0x09 */
    KEY_9,              /* 0x0A */
    KEY_0,              /* 0x0B */
    KEY_MINUS,          /* 0x0C */
    KEY_EQUAL,          /* 0x0D */
    KEY_BACKSPACE,      /* 0x0E */
    KEY_TAB,            /* 0x0F */
    KEY_Q,              /* 0x10 */
    KEY_W,              /* 0x11 */
    KEY_E,              /* 0x12 */
    KEY_R,              /* 0x13 */
    KEY_T,              /* 0x14 */
    KEY_Y,              /* 0x15 */
    KEY_U,              /* 0x16 */
    KEY_I,              /* 0x17 */
    KEY_O,              /* 0x18 */
    KEY_P,              /* 0x19 */
    KEY_LEFT_BRACE,     /* 0x1A */
    KEY_RIGHT_BRACE,    /* 0x1B */
    KEY_ENTER,          /* 0x1C */
    KEY_LEFT_CTRL,      /* 0x1D */
    KEY_A,              /* 0x1E */
    KEY_S,              /* 0x1F */
    KEY_D,              /* 0x20 */
    KEY_F,              /* 0x21 */
    KEY_G,              /* 0x22 */
    KEY_H,              /* 0x23 */
    KEY_J,              /* 0x24 */
    KEY_K,              /* 0x25 */
    KEY_L,              /* 0x26 */
    KEY_SEMICOLON,      /* 0x27 */
    KEY_APOSTROPHE,     /* 0x28 */
    KEY_GRAVE,          /* 0x29 */
    KEY_LEFT_SHIFT,     /* 0x2A */
    KEY_BACKSLASH,      /* 0x2B */
    KEY_Z,              /* 0x2C */
    KEY_X,              /* 0x2D */
    KEY_C,              /* 0x2E */
    KEY_V,              /* 0x2F */
    KEY_B,              /* 0x30 */
    KEY_N,              /* 0x31 */
    KEY_M,              /* 0x32 */
    KEY_COMMA,          /* 0x33 */
    KEY_DOT,            /* 0x34 */
    KEY_SLASH,          /* 0x35 */
    KEY_RIGHT_SHIFT,    /* 0x36 */
    KEY_KP_MULTIPLY,    /* 0x37 */
    KEY_LEFT_ALT,       /* 0x38 */
    KEY_SPACE,          /* 0x39 */
    KEY_CAPS_LOCK,      /* 0x3A */
    KEY_F1,             /* 0x3B */
    KEY_F2,             /* 0x3C */
    KEY_F3,             /* 0x3D */
    KEY_F4,             /* 0x3E */
    KEY_F5,             /* 0x3F */
    KEY_F6,             /* 0x40 */
    KEY_F7,             /* 0x41 */
    KEY_F8,             /* 0x42 */
    KEY_F9,             /* 0x43 */
    KEY_F10,            /* 0x44 */
    KEY_NUM_LOCK,       /* 0x45 */
    KEY_SCROLL_LOCK,    /* 0x46 */
    KEY_KP_7,           /* 0x47 */
    KEY_KP_8,           /* 0x48 */
    KEY_KP_9,           /* 0x49 */
    KEY_KP_SUBTRACT,    /* 0x4A */
    KEY_KP_4,           /* 0x4B */
    KEY_KP_5,           /* 0x4C */
    KEY_KP_6,           /* 0x4D */
    KEY_KP_ADD,         /* 0x4E */
    KEY_KP_1,           /* 0x4F */
    KEY_KP_2,           /* 0x50 */
    KEY_KP_3,           /* 0x51 */
    KEY_KP_0,           /* 0x52 */
    KEY_KP_DOT,         /* 0x53 */
    KEY_UNKNOWN,        /* 0x54 */
    KEY_UNKNOWN,        /* 0x55 */
    KEY_UNKNOWN,        /* 0x56 */
    KEY_F11,            /* 0x57 */
    KEY_F12,            /* 0x58 */
    KEY_UNKNOWN,        /* 0x59 */
    KEY_UNKNOWN,        /* 0x5A */
    KEY_UNKNOWN,        /* 0x5B - Left Win (handled specially) */
    KEY_UNKNOWN,        /* 0x5C - Right Win (handled specially) */
    KEY_APPLICATION,    /* 0x5D - Menu key */
    KEY_UNKNOWN,        /* 0x5E */
    KEY_UNKNOWN,        /* 0x5F */
    KEY_UNKNOWN,        /* 0x60 */
    KEY_UNKNOWN,        /* 0x61 */
    KEY_UNKNOWN,        /* 0x62 */
    KEY_UNKNOWN,        /* 0x63 */
    KEY_UNKNOWN,        /* 0x64 */
    KEY_UNKNOWN,        /* 0x65 */
    KEY_UNKNOWN,        /* 0x66 */
    KEY_UNKNOWN,        /* 0x67 */
    KEY_UNKNOWN,        /* 0x68 */
    KEY_UNKNOWN,        /* 0x69 */
    KEY_UNKNOWN,        /* 0x6A */
    KEY_UNKNOWN,        /* 0x6B */
    KEY_UNKNOWN,        /* 0x6C */
    KEY_UNKNOWN,        /* 0x6D */
    KEY_UNKNOWN,        /* 0x6E */
    KEY_UNKNOWN,        /* 0x6F */
    KEY_UNKNOWN,        /* 0x70 */
    KEY_UNKNOWN,        /* 0x71 */
    KEY_UNKNOWN,        /* 0x72 */
    KEY_UNKNOWN,        /* 0x73 */
    KEY_UNKNOWN,        /* 0x74 */
    KEY_UNKNOWN,        /* 0x75 */
    KEY_UNKNOWN,        /* 0x76 */
    KEY_UNKNOWN,        /* 0x77 */
    KEY_UNKNOWN,        /* 0x78 */
    KEY_UNKNOWN,        /* 0x79 */
    KEY_UNKNOWN,        /* 0x7A */
    KEY_UNKNOWN,        /* 0x7B */
    KEY_UNKNOWN,        /* 0x7C */
    KEY_UNKNOWN,        /* 0x7D */
    KEY_UNKNOWN,        /* 0x7E */
    KEY_UNKNOWN         /* 0x7F */
};

/* Extended scancode to keycode mapping (E0 prefix) */
static const uint16_t extended_scancode_to_keycode[128] = {
    KEY_UNKNOWN,        /* 0x00 */
    KEY_UNKNOWN,        /* 0x01 */
    KEY_UNKNOWN,        /* 0x02 */
    KEY_UNKNOWN,        /* 0x03 */
    KEY_UNKNOWN,        /* 0x04 */
    KEY_UNKNOWN,        /* 0x05 */
    KEY_UNKNOWN,        /* 0x06 */
    KEY_UNKNOWN,        /* 0x07 */
    KEY_UNKNOWN,        /* 0x08 */
    KEY_UNKNOWN,        /* 0x09 */
    KEY_UNKNOWN,        /* 0x0A */
    KEY_UNKNOWN,        /* 0x0B */
    KEY_UNKNOWN,        /* 0x0C */
    KEY_UNKNOWN,        /* 0x0D */
    KEY_UNKNOWN,        /* 0x0E */
    KEY_UNKNOWN,        /* 0x0F */
    KEY_UNKNOWN,        /* 0x10 */
    KEY_UNKNOWN,        /* 0x11 */
    KEY_UNKNOWN,        /* 0x12 */
    KEY_UNKNOWN,        /* 0x13 */
    KEY_UNKNOWN,        /* 0x14 */
    KEY_UNKNOWN,        /* 0x15 */
    KEY_UNKNOWN,        /* 0x16 */
    KEY_UNKNOWN,        /* 0x17 */
    KEY_UNKNOWN,        /* 0x18 */
    KEY_UNKNOWN,        /* 0x19 */
    KEY_UNKNOWN,        /* 0x1A */
    KEY_UNKNOWN,        /* 0x1B */
    KEY_KP_ENTER,       /* 0x1C */
    KEY_RIGHT_CTRL,     /* 0x1D */
    KEY_UNKNOWN,        /* 0x1E */
    KEY_UNKNOWN,        /* 0x1F */
    KEY_UNKNOWN,        /* 0x20 */
    KEY_UNKNOWN,        /* 0x21 */
    KEY_UNKNOWN,        /* 0x22 */
    KEY_UNKNOWN,        /* 0x23 */
    KEY_UNKNOWN,        /* 0x24 */
    KEY_UNKNOWN,        /* 0x25 */
    KEY_UNKNOWN,        /* 0x26 */
    KEY_UNKNOWN,        /* 0x27 */
    KEY_UNKNOWN,        /* 0x28 */
    KEY_UNKNOWN,        /* 0x29 */
    KEY_UNKNOWN,        /* 0x2A */
    KEY_UNKNOWN,        /* 0x2B */
    KEY_UNKNOWN,        /* 0x2C */
    KEY_UNKNOWN,        /* 0x2D */
    KEY_UNKNOWN,        /* 0x2E */
    KEY_UNKNOWN,        /* 0x2F */
    KEY_UNKNOWN,        /* 0x30 */
    KEY_UNKNOWN,        /* 0x31 */
    KEY_UNKNOWN,        /* 0x32 */
    KEY_UNKNOWN,        /* 0x33 */
    KEY_UNKNOWN,        /* 0x34 */
    KEY_KP_DIVIDE,      /* 0x35 */
    KEY_UNKNOWN,        /* 0x36 */
    KEY_PRINT_SCREEN,   /* 0x37 */
    KEY_RIGHT_ALT,      /* 0x38 */
    KEY_UNKNOWN,        /* 0x39 */
    KEY_UNKNOWN,        /* 0x3A */
    KEY_UNKNOWN,        /* 0x3B */
    KEY_UNKNOWN,        /* 0x3C */
    KEY_UNKNOWN,        /* 0x3D */
    KEY_UNKNOWN,        /* 0x3E */
    KEY_UNKNOWN,        /* 0x3F */
    KEY_UNKNOWN,        /* 0x40 */
    KEY_UNKNOWN,        /* 0x41 */
    KEY_UNKNOWN,        /* 0x42 */
    KEY_UNKNOWN,        /* 0x43 */
    KEY_UNKNOWN,        /* 0x44 */
    KEY_UNKNOWN,        /* 0x45 */
    KEY_UNKNOWN,        /* 0x46 */
    KEY_HOME,           /* 0x47 */
    KEY_UP,             /* 0x48 */
    KEY_PAGE_UP,        /* 0x49 */
    KEY_UNKNOWN,        /* 0x4A */
    KEY_LEFT,           /* 0x4B */
    KEY_UNKNOWN,        /* 0x4C */
    KEY_RIGHT,          /* 0x4D */
    KEY_UNKNOWN,        /* 0x4E */
    KEY_END,            /* 0x4F */
    KEY_DOWN,           /* 0x50 */
    KEY_PAGE_DOWN,      /* 0x51 */
    KEY_INSERT,         /* 0x52 */
    KEY_DELETE,         /* 0x53 */
    KEY_UNKNOWN,        /* 0x54 */
    KEY_UNKNOWN,        /* 0x55 */
    KEY_UNKNOWN,        /* 0x56 */
    KEY_UNKNOWN,        /* 0x57 */
    KEY_UNKNOWN,        /* 0x58 */
    KEY_UNKNOWN,        /* 0x59 */
    KEY_UNKNOWN,        /* 0x5A */
    KEY_UNKNOWN,        /* 0x5B - Left Win */
    KEY_UNKNOWN,        /* 0x5C - Right Win */
    KEY_UNKNOWN,        /* 0x5D - Menu */
    KEY_UNKNOWN,        /* 0x5E */
    KEY_UNKNOWN,        /* 0x5F */
    KEY_UNKNOWN,        /* 0x60 */
    KEY_UNKNOWN,        /* 0x61 */
    KEY_UNKNOWN,        /* 0x62 */
    KEY_UNKNOWN,        /* 0x63 */
    KEY_UNKNOWN,        /* 0x64 */
    KEY_UNKNOWN,        /* 0x65 */
    KEY_UNKNOWN,        /* 0x66 */
    KEY_UNKNOWN,        /* 0x67 */
    KEY_UNKNOWN,        /* 0x68 */
    KEY_UNKNOWN,        /* 0x69 */
    KEY_UNKNOWN,        /* 0x6A */
    KEY_UNKNOWN,        /* 0x6B */
    KEY_UNKNOWN,        /* 0x6C */
    KEY_UNKNOWN,        /* 0x6D */
    KEY_UNKNOWN,        /* 0x6E */
    KEY_UNKNOWN,        /* 0x6F */
    KEY_UNKNOWN,        /* 0x70 */
    KEY_UNKNOWN,        /* 0x71 */
    KEY_UNKNOWN,        /* 0x72 */
    KEY_UNKNOWN,        /* 0x73 */
    KEY_UNKNOWN,        /* 0x74 */
    KEY_UNKNOWN,        /* 0x75 */
    KEY_UNKNOWN,        /* 0x76 */
    KEY_UNKNOWN,        /* 0x77 */
    KEY_UNKNOWN,        /* 0x78 */
    KEY_UNKNOWN,        /* 0x79 */
    KEY_UNKNOWN,        /* 0x7A */
    KEY_UNKNOWN,        /* 0x7B */
    KEY_UNKNOWN,        /* 0x7C */
    KEY_UNKNOWN,        /* 0x7D */
    KEY_UNKNOWN,        /* 0x7E */
    KEY_UNKNOWN         /* 0x7F */
};

/* Queue a key event */
static void keyboard_queue_event(uint16_t keycode, char ascii, key_event_type_t type, bool extended) {
    int next = (keyboard_queue_head + 1) % KEYBOARD_QUEUE_SIZE;
    if (next != keyboard_queue_tail) {
        keyboard_queue[keyboard_queue_head].scancode = (uint8_t)(keycode & 0xFF);
        keyboard_queue[keyboard_queue_head].keycode = keycode;
        keyboard_queue[keyboard_queue_head].ascii = ascii;
        keyboard_queue[keyboard_queue_head].type = type;
        keyboard_queue[keyboard_queue_head].modifiers = keyboard_modifiers;
        keyboard_queue[keyboard_queue_head].extended = extended;
        keyboard_queue_head = next;
    }
    
    /* Also push to Rust GUI event queue */
    uint8_t rust_type = 0;
    switch (type) {
        case KEY_EVENT_DOWN: rust_type = 1; break;
        case KEY_EVENT_UP: rust_type = 2; break;
        case KEY_EVENT_REPEAT: rust_type = 3; break;
    }
    rust_gui_push_key_event(rust_type, (uint8_t)(keycode & 0xFF), (uint8_t)ascii, keyboard_modifiers);
}

/* USB HID entry point: push a keycode into the same event queue the PS/2 path
 * uses. keycodes are USB HID usage IDs, so they map 1:1 to KEY_* values. */
void keyboard_queue_key(uint16_t keycode, char ascii, uint8_t type) {
    key_event_type_t t = (key_event_type_t)type;
    if (t > KEY_EVENT_REPEAT) t = KEY_EVENT_DOWN;
    keyboard_queue_event(keycode, ascii, t, false);
}

/* Update keyboard LEDs */
static void keyboard_update_leds(void) {
    uint8_t leds = 0;
    if (keyboard_modifiers & KMOD_SCROLL) leds |= KBD_LED_SCROLL_LOCK;
    if (keyboard_modifiers & KMOD_NUM)    leds |= KBD_LED_NUM_LOCK;
    if (keyboard_modifiers & KMOD_CAPS)   leds |= KBD_LED_CAPS_LOCK;
    
    /* Send LED command */
    outb(KBD_CMD_PORT, KBD_CMD_SET_LEDS);
    while (inb(KBD_STATUS_PORT) & KBD_STATUS_IBF);
    outb(KBD_DATA_PORT, leds);
    while (inb(KBD_STATUS_PORT) & KBD_STATUS_IBF);
}

/* Handle key press */
static void keyboard_handle_key_down(uint16_t keycode, bool extended) {
    if (keycode >= MAX_KEYS_TRACKED) return;
    
    if (!key_pressed[keycode]) {
        key_pressed[keycode] = true;
        key_press_time[keycode] = timer_get_ticks();
        key_repeat_sent[keycode] = false;
        
        /* Handle modifier keys */
        switch (keycode) {
            case KEY_LEFT_SHIFT:
            case KEY_RIGHT_SHIFT:
                keyboard_modifiers |= KMOD_SHIFT;
                break;
            case KEY_LEFT_CTRL:
            case KEY_RIGHT_CTRL:
                keyboard_modifiers |= KMOD_CTRL;
                break;
            case KEY_LEFT_ALT:
            case KEY_RIGHT_ALT:
                keyboard_modifiers |= KMOD_ALT;
                break;
            case KEY_LEFT_META:
            case KEY_RIGHT_META:
                keyboard_modifiers |= KMOD_META;
                break;
            case KEY_CAPS_LOCK:
                keyboard_modifiers ^= KMOD_CAPS;
                keyboard_update_leds();
                break;
            case KEY_NUM_LOCK:
                keyboard_modifiers ^= KMOD_NUM;
                keyboard_update_leds();
                break;
            case KEY_SCROLL_LOCK:
                keyboard_modifiers ^= KMOD_SCROLL;
                keyboard_update_leds();
                break;
        }
        
        char ascii = keyboard_keycode_to_ascii(keycode, keyboard_modifiers);
        keyboard_queue_event(keycode, ascii, KEY_EVENT_DOWN, extended);
    }
}

/* Handle key release */
static void keyboard_handle_key_up(uint16_t keycode, bool extended) {
    if (keycode >= MAX_KEYS_TRACKED) return;
    
    if (key_pressed[keycode]) {
        key_pressed[keycode] = false;
        key_repeat_sent[keycode] = false;
        
        /* Handle modifier keys */
        switch (keycode) {
            case KEY_LEFT_SHIFT:
            case KEY_RIGHT_SHIFT:
                keyboard_modifiers &= ~KMOD_SHIFT;
                break;
            case KEY_LEFT_CTRL:
            case KEY_RIGHT_CTRL:
                keyboard_modifiers &= ~KMOD_CTRL;
                break;
            case KEY_LEFT_ALT:
            case KEY_RIGHT_ALT:
                keyboard_modifiers &= ~KMOD_ALT;
                break;
            case KEY_LEFT_META:
            case KEY_RIGHT_META:
                keyboard_modifiers &= ~KMOD_META;
                break;
        }
        
        keyboard_queue_event(keycode, 0, KEY_EVENT_UP, extended);
    }
}

/* Process keyboard repeat */
void keyboard_process_repeat(void) {
    if (!repeat_enabled) return;
    
    uint32_t now = timer_get_ticks();
    
    for (int i = 0; i < MAX_KEYS_TRACKED; i++) {
        if (key_pressed[i] && !key_repeat_sent[i]) {
            uint32_t elapsed = now - key_press_time[i];
            if (elapsed >= repeat_delay_ms) {
                key_repeat_sent[i] = true;
                char ascii = keyboard_keycode_to_ascii(i, keyboard_modifiers);
                keyboard_queue_event(i, ascii, KEY_EVENT_REPEAT, false);
            }
        } else if (key_pressed[i] && key_repeat_sent[i]) {
            uint32_t elapsed = now - key_press_time[i];
            if (elapsed >= repeat_delay_ms + repeat_rate_ms) {
                /* Calculate how many repeats should have fired */
                uint32_t repeats = (elapsed - repeat_delay_ms) / repeat_rate_ms;
                /* For simplicity, just send one repeat per call if enough time passed */
                uint32_t last_repeat_time = key_press_time[i] + repeat_delay_ms + 
                                           ((repeats - 1) * repeat_rate_ms);
                if (now >= last_repeat_time + repeat_rate_ms) {
                    char ascii = keyboard_keycode_to_ascii(i, keyboard_modifiers);
                    keyboard_queue_event(i, ascii, KEY_EVENT_REPEAT, false);
                }
            }
        }
    }
}

/* Keyboard interrupt handler */
static void keyboard_callback(registers_t *regs) {
    (void)regs;
    /* IRQ entry hook for GUI input ring */
    
    
    uint8_t status = inb(KBD_STATUS_PORT);
    if (!(status & KBD_STATUS_OBF)) return;
    
    uint8_t scancode = inb(KBD_DATA_PORT);
    
    /* Handle pause key sequence (E1 1D 45 E1 9D C5) */
    if (pause_sequence > 0) {
        if (pause_sequence == 1 && scancode == 0x1D) { pause_sequence = 2; return; }
        if (pause_sequence == 2 && scancode == 0x45) { pause_sequence = 3; return; }
        if (pause_sequence == 3 && scancode == 0xE1) { pause_sequence = 4; return; }
        if (pause_sequence == 4 && scancode == 0x9D) { pause_sequence = 5; return; }
        if (pause_sequence == 5 && scancode == 0xC5) {
            /* Pause key pressed */
            keyboard_handle_key_down(KEY_PAUSE, false);
            keyboard_handle_key_up(KEY_PAUSE, false);
        }
        pause_sequence = 0;
        return;
    }
    
    /* Handle E0 prefix (extended scancode) */
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }
    
    /* Handle E1 prefix (pause key) */
    if (scancode == 0xE1) {
        pause_sequence = 1;
        return;
    }
    
    bool key_up = (scancode & 0x80) != 0;
    uint8_t base_scancode = scancode & 0x7F;
    
    uint16_t keycode;
    if (extended_scancode) {
        if (base_scancode < 128) {
            keycode = extended_scancode_to_keycode[base_scancode];
        } else {
            keycode = KEY_UNKNOWN;
        }
        
        /* Special handling for Win keys in extended scancodes */
        if (base_scancode == 0x5B) keycode = KEY_LEFT_META;
        if (base_scancode == 0x5C) keycode = KEY_RIGHT_META;
        if (base_scancode == 0x5D) keycode = KEY_APPLICATION;
        
        extended_scancode = false;
    } else {
        if (base_scancode < 128) {
            keycode = scancode_to_keycode[base_scancode];
        } else {
            keycode = KEY_UNKNOWN;
        }
        
        /* Special handling for Win keys in non-extended scancodes */
        if (base_scancode == 0x5B) keycode = KEY_LEFT_META;
        if (base_scancode == 0x5C) keycode = KEY_RIGHT_META;
    }
    
    if (keycode != KEY_UNKNOWN) {
        if (key_up) {
            keyboard_handle_key_up(keycode, extended_scancode);
        } else {
            keyboard_handle_key_down(keycode, extended_scancode);
        }
    }
}

/* Bounded wait helpers (avoid infinite spins in case a device never answers) */
static void kbd_wait_ibf(void) {
    int timeout = 100000;
    while (timeout-- && (inb(KBD_STATUS_PORT) & KBD_STATUS_IBF));
}

static void kbd_wait_obf(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF));
}

/* Send a command to the KEYBOARD DEVICE via the data port (0x60).
 * Device commands must NEVER be written to the controller command port
 * (0x64): on real hardware it corrupts the controller state and on QEMU
 * unknown controller commands can trigger a machine reset. */
static uint8_t kbd_device_command(uint8_t cmd) {
    kbd_wait_ibf();
    outb(KBD_DATA_PORT, cmd);
    kbd_wait_obf();
    return inb(KBD_DATA_PORT);
}

void keyboard_init(void) {
    /* Reset keyboard (device answers: ACK 0xFA, then BAT 0xAA, then ACK 0xFA) */
    kbd_device_command(KBD_CMD_RESET);

    /* Drain any remaining responses */
    int drain = 8;
    while (drain-- && (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) {
        inb(KBD_DATA_PORT);
    }

    /* Disable repeat to avoid flooding */
    keyboard_set_repeat_enabled(false);

    /* Enable scanning */
    kbd_device_command(KBD_CMD_ENABLE);
    drain = 4;
    while (drain-- && (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) {
        inb(KBD_DATA_PORT);
    }

    /* Clear state */
    keyboard_queue_head = 0;
    keyboard_queue_tail = 0;
    keyboard_modifiers = 0;
    extended_scancode = false;
    pause_sequence = 0;
    
    for (int i = 0; i < MAX_KEYS_TRACKED; i++) {
        key_pressed[i] = false;
        key_press_time[i] = 0;
        key_repeat_sent[i] = false;
    }
    
    isr_register_handler(IRQ1, keyboard_callback);
    pic_clear_mask(1);
    
    kprintf("[KEYBOARD] Enhanced driver initialized with repeat and extended scancode support\n");
}

bool keyboard_get_event(key_event_t *event) {
    /* Process repeat before getting event */
    keyboard_process_repeat();
    
    if (keyboard_queue_head == keyboard_queue_tail) return false;
    
    *event = keyboard_queue[keyboard_queue_tail];
    keyboard_queue_tail = (keyboard_queue_tail + 1) % KEYBOARD_QUEUE_SIZE;
    return true;
}

uint8_t keyboard_get_modifiers(void) {
    return keyboard_modifiers;
}

bool keyboard_is_key_pressed(uint16_t keycode) {
    if (keycode >= MAX_KEYS_TRACKED) return false;
    return key_pressed[keycode];
}

void keyboard_set_repeat_rate(uint32_t delay_ms, uint32_t rate_ms) {
    repeat_delay_ms = delay_ms;
    repeat_rate_ms = rate_ms;
}

uint16_t keyboard_scancode_to_keycode(uint8_t scancode, bool extended) {
    if (extended) {
        return extended_scancode_to_keycode[scancode & 0x7F];
    }
    return scancode_to_keycode[scancode & 0x7F];
}

char keyboard_keycode_to_ascii(uint16_t keycode, uint8_t modifiers) {
    bool shift = (modifiers & KMOD_SHIFT) != 0;
    bool caps = (modifiers & KMOD_CAPS) != 0;
    
    /* Letters */
    if (keycode >= KEY_A && keycode <= KEY_Z) {
        char c = 'a' + (keycode - KEY_A);
        if (shift ^ caps) c = c - 'a' + 'A';
        return c;
    }
    
    /* Numbers and symbols */
    switch (keycode) {
        case KEY_1: return shift ? '!' : '1';
        case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3';
        case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5';
        case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7';
        case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9';
        case KEY_0: return shift ? ')' : '0';
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFT_BRACE: return shift ? '{' : '[';
        case KEY_RIGHT_BRACE: return shift ? '}' : ']';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_HASHTILDE: return shift ? '~' : '`';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';
        case KEY_SPACE: return ' ';
        case KEY_TAB: return '\t';
        case KEY_ENTER: return '\n';
        case KEY_KP_0: return '0';
        case KEY_KP_1: return '1';
        case KEY_KP_2: return '2';
        case KEY_KP_3: return '3';
        case KEY_KP_4: return '4';
        case KEY_KP_5: return '5';
        case KEY_KP_6: return '6';
        case KEY_KP_7: return '7';
        case KEY_KP_8: return '8';
        case KEY_KP_9: return '9';
        case KEY_KP_DOT: return '.';
        case KEY_KP_DIVIDE: return '/';
        case KEY_KP_MULTIPLY: return '*';
        case KEY_KP_SUBTRACT: return '-';
        case KEY_KP_ADD: return '+';
        case KEY_KP_ENTER: return '\n';
    }
    
    return 0;
}

void keyboard_set_repeat_enabled(bool enabled) {
    repeat_enabled = enabled;
}

/* Legacy compatibility */
char keyboard_getchar(void) {
    key_event_t event;
    while (keyboard_get_event(&event)) {
        if (event.type == KEY_EVENT_DOWN && event.ascii != 0) {
            return event.ascii;
        }
    }
    return 0;
}