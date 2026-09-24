#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"

/* Keyboard modifiers */
#define KMOD_SHIFT      0x01
#define KMOD_CTRL       0x02
#define KMOD_ALT        0x04
#define KMOD_META       0x08  /* Win key */
#define KMOD_CAPS       0x10
#define KMOD_NUM        0x20
#define KMOD_SCROLL     0x40

/* Key event types */
typedef enum {
    KEY_EVENT_DOWN = 0,
    KEY_EVENT_UP,
    KEY_EVENT_REPEAT
} key_event_type_t;

/* Key event structure */
typedef struct key_event {
    uint8_t scancode;           /* Raw scancode (with E0 prefix handled) */
    uint16_t keycode;           /* Normalized key code */
    char ascii;                 /* ASCII character (0 if none) */
    key_event_type_t type;      /* Down, up, or repeat */
    uint8_t modifiers;          /* Active modifiers at time of event */
    bool extended;              /* True if E0-prefixed scancode */
} key_event_t;

/* Standard key codes (USB HID usage page compatible) */
#define KEY_UNKNOWN       0x0000
#define KEY_A             0x0004
#define KEY_B             0x0005
#define KEY_C             0x0006
#define KEY_D             0x0007
#define KEY_E             0x0008
#define KEY_F             0x0009
#define KEY_G             0x000A
#define KEY_H             0x000B
#define KEY_I             0x000C
#define KEY_J             0x000D
#define KEY_K             0x000E
#define KEY_L             0x000F
#define KEY_M             0x0010
#define KEY_N             0x0011
#define KEY_O             0x0012
#define KEY_P             0x0013
#define KEY_Q             0x0014
#define KEY_R             0x0015
#define KEY_S             0x0016
#define KEY_T             0x0017
#define KEY_U             0x0018
#define KEY_V             0x0019
#define KEY_W             0x001A
#define KEY_X             0x001B
#define KEY_Y             0x001C
#define KEY_Z             0x001D

#define KEY_1             0x001E
#define KEY_2             0x001F
#define KEY_3             0x0020
#define KEY_4             0x0021
#define KEY_5             0x0022
#define KEY_6             0x0023
#define KEY_7             0x0024
#define KEY_8             0x0025
#define KEY_9             0x0026
#define KEY_0             0x0027

#define KEY_ENTER         0x0028
#define KEY_ESCAPE        0x0029
#define KEY_BACKSPACE     0x002A
#define KEY_TAB           0x002B
#define KEY_SPACE         0x002C
#define KEY_MINUS         0x002D
#define KEY_EQUAL         0x002E
#define KEY_LEFT_BRACE    0x002F
#define KEY_RIGHT_BRACE   0x0030
#define KEY_BACKSLASH     0x0031
#define KEY_HASHTILDE     0x0032
#define KEY_SEMICOLON     0x0033
#define KEY_APOSTROPHE    0x0034
#define KEY_GRAVE         0x0035
#define KEY_COMMA         0x0036
#define KEY_DOT           0x0037
#define KEY_SLASH         0x0038
#define KEY_CAPS_LOCK     0x0039

#define KEY_F1            0x003A
#define KEY_F2            0x003B
#define KEY_F3            0x003C
#define KEY_F4            0x003D
#define KEY_F5            0x003E
#define KEY_F6            0x003F
#define KEY_F7            0x0040
#define KEY_F8            0x0041
#define KEY_F9            0x0042
#define KEY_F10           0x0043
#define KEY_F11           0x0044
#define KEY_F12           0x0045

#define KEY_PRINT_SCREEN  0x0046
#define KEY_SCROLL_LOCK   0x0047
#define KEY_PAUSE         0x0048
#define KEY_INSERT        0x0049
#define KEY_HOME          0x004A
#define KEY_PAGE_UP       0x004B
#define KEY_DELETE        0x004C
#define KEY_END           0x004D
#define KEY_PAGE_DOWN     0x004E
#define KEY_RIGHT         0x004F
#define KEY_LEFT          0x0050
#define KEY_DOWN          0x0051
#define KEY_UP            0x0052

#define KEY_NUM_LOCK      0x0053
#define KEY_KP_DIVIDE     0x0054
#define KEY_KP_MULTIPLY   0x0055
#define KEY_KP_SUBTRACT   0x0056
#define KEY_KP_ADD        0x0057
#define KEY_KP_ENTER      0x0058
#define KEY_KP_1          0x0059
#define KEY_KP_2          0x005A
#define KEY_KP_3          0x005B
#define KEY_KP_4          0x005C
#define KEY_KP_5          0x005D
#define KEY_KP_6          0x005E
#define KEY_KP_7          0x005F
#define KEY_KP_8          0x0060
#define KEY_KP_9          0x0061
#define KEY_KP_0          0x0062
#define KEY_KP_DOT        0x0063

#define KEY_NON_US_BACKSLASH 0x0064
#define KEY_APPLICATION      0x0065
#define KEY_POWER            0x0066
#define KEY_KP_EQUALS        0x0067

#define KEY_F13            0x0068
#define KEY_F14            0x0069
#define KEY_F15            0x006A
#define KEY_F16            0x006B
#define KEY_F17            0x006C
#define KEY_F18            0x006D
#define KEY_F19            0x006E
#define KEY_F20            0x006F
#define KEY_F21            0x0070
#define KEY_F22            0x0071
#define KEY_F23            0x0072
#define KEY_F24            0x0073

#define KEY_EXECUTE        0x0074
#define KEY_HELP           0x0075
#define KEY_MENU           0x0076
#define KEY_SELECT         0x0077
#define KEY_STOP           0x0078
#define KEY_AGAIN          0x0079
#define KEY_UNDO           0x007A
#define KEY_CUT            0x007B
#define KEY_COPY           0x007C
#define KEY_PASTE          0x007D
#define KEY_FIND           0x007E
#define KEY_MUTE           0x007F
#define KEY_VOLUME_UP      0x0080
#define KEY_VOLUME_DOWN    0x0081

#define KEY_KP_COMMA       0x0085
#define KEY_KP_EQUALS_AS400 0x0086

#define KEY_INTERNATIONAL1 0x0087
#define KEY_INTERNATIONAL2 0x0088
#define KEY_INTERNATIONAL3 0x0089
#define KEY_INTERNATIONAL4 0x008A
#define KEY_INTERNATIONAL5 0x008B
#define KEY_INTERNATIONAL6 0x008C
#define KEY_INTERNATIONAL7 0x008D
#define KEY_INTERNATIONAL8 0x008E
#define KEY_INTERNATIONAL9 0x008F
#define KEY_LANG1          0x0090
#define KEY_LANG2          0x0091
#define KEY_LANG3          0x0092
#define KEY_LANG4          0x0093
#define KEY_LANG5          0x0094
#define KEY_LANG6          0x0095
#define KEY_LANG7          0x0096
#define KEY_LANG8          0x0097
#define KEY_LANG9          0x0098

#define KEY_ALT_ERASE      0x0099
#define KEY_SYSREQ         0x009A
#define KEY_CANCEL         0x009B
#define KEY_CLEAR          0x009C
#define KEY_PRIOR          0x009D
#define KEY_RETURN2        0x009E
#define KEY_SEPARATOR      0x009F
#define KEY_OUT            0x00A0
#define KEY_OPER           0x00A1
#define KEY_CLEAR_AGAIN    0x00A2
#define KEY_CRSEL          0x00A3
#define KEY_EXSEL          0x00A4

/* Modifier key codes */
#define KEY_LEFT_CTRL      0x00E0
#define KEY_LEFT_SHIFT     0x00E1
#define KEY_LEFT_ALT       0x00E2
#define KEY_LEFT_META      0x00E3  /* Left Win */
#define KEY_RIGHT_CTRL     0x00E4
#define KEY_RIGHT_SHIFT    0x00E5
#define KEY_RIGHT_ALT      0x00E6
#define KEY_RIGHT_META     0x00E7  /* Right Win */

/* Media keys (extended) */
#define KEY_MEDIA_PLAY_PAUSE   0x0100
#define KEY_MEDIA_STOP         0x0101
#define KEY_MEDIA_PREV         0x0102
#define KEY_MEDIA_NEXT         0x0103
#define KEY_MEDIA_EJECT        0x0104
#define KEY_MEDIA_VOLUME_UP    0x0105
#define KEY_MEDIA_VOLUME_DOWN  0x0106
#define KEY_MEDIA_MUTE         0x0107
#define KEY_MEDIA_WWW_HOME     0x0108
#define KEY_MEDIA_WWW_SEARCH   0x0109
#define KEY_MEDIA_WWW_FAV      0x010A
#define KEY_MEDIA_WWW_REFRESH  0x010B
#define KEY_MEDIA_WWW_STOP     0x010C
#define KEY_MEDIA_WWW_FORWARD  0x010D
#define KEY_MEDIA_WWW_BACK     0x010E
#define KEY_MEDIA_MAIL         0x010F
#define KEY_MEDIA_CALC         0x0110
#define KEY_MEDIA_COMPUTER     0x0111
#define KEY_MEDIA_SLEEP        0x0112

/* Keyboard repeat settings */
#define KEY_REPEAT_DELAY_MS    500   /* Initial delay before repeat */
#define KEY_REPEAT_RATE_MS     30    /* Repeat interval */

/* Initialize keyboard driver */
void keyboard_init(void);

/* Get next key event from queue (non-blocking) */
bool keyboard_get_event(key_event_t *event);

/* Get current modifier state */
uint8_t keyboard_get_modifiers(void);

/* Check if a specific key is currently pressed */
bool keyboard_is_key_pressed(uint16_t keycode);

/* Set keyboard repeat rate (delay in ms, rate in ms) */
void keyboard_set_repeat_rate(uint32_t delay_ms, uint32_t rate_ms);

/* Convert scancode to keycode */
uint16_t keyboard_scancode_to_keycode(uint8_t scancode, bool extended);

/* Convert keycode to ASCII with modifiers */
char keyboard_keycode_to_ascii(uint16_t keycode, uint8_t modifiers);

/* Enable/disable keyboard repeat */
void keyboard_set_repeat_enabled(bool enabled);

/* USB HID entry point: queue a key event directly (keycode = USB HID usage id,
 * ascii = pre-computed character, type = KEY_EVENT_DOWN/UP/REPEAT). */
void keyboard_queue_key(uint16_t keycode, char ascii, uint8_t type);

#endif