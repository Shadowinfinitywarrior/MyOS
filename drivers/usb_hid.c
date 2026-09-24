#include "usb_hid.h"
#include "keyboard.h"
#include "mouse.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define USB_HID_MAX_SLOTS 65

static usb_hid_dev_t usb_hid_devs[USB_HID_MAX_SLOTS];
static bool usb_hid_devset[USB_HID_MAX_SLOTS];

/* Per-slot key tracker for press/release edge detection */
static uint8_t usb_hid_last_keys[USB_HID_MAX_SLOTS][6];
static uint8_t usb_hid_key_count[USB_HID_MAX_SLOTS];

int usb_hid_init(void) {
    kprintf("[HID] HID decoder ready (boot protocol keyboard/mouse)\n");
    return 0;
}

int usb_hid_poll(void) {
    return 0;
}

/* Walk the config descriptor looking for the HID interface and its first
 * interrupt IN endpoint. Descriptor data is read in raw byte form. */
int usb_hid_parse_cfgdesc(const uint8_t *c, uint16_t len, usb_hid_dev_t *dev) {
    if (!c || !dev || len < 9) return 0;

    uint16_t total = c[2] | (uint16_t)(c[3] << 8);
    if (total == 0 || total > len) total = len;

    uint16_t off = 0;
    uint8_t if_class = 0;
    uint8_t if_prot  = 0;

    while (off + 2 <= total) {
        uint8_t bl = c[off];
        uint8_t bt = c[off + 1];
        if (bl == 0 || off + bl > total)
            break;

        if (bt == 4 && bl >= 9) {
            /* Interface descriptor */
            if_class = c[off + 5];
            if_prot  = c[off + 7];
        } else if (bt == 5 && bl >= 7) {
            /* Endpoint descriptor */
            uint8_t addr  = c[off + 2];
            uint8_t attr  = c[off + 3];
            if ((attr & 0x03) == 0x03 && (addr & 0x80) && if_class == 0x03) {
                /* interrupt IN endpoint inside an HID interface */
                dev->ep_addr   = addr;
                dev->max_packet = c[off + 4] | (uint16_t)(c[off + 5] << 8);
                dev->interval  = c[off + 6];
                /* QEMU uses subclass 1 for both boot kbd+mouse;
                 * distinguish by boot protocol (HID 1.11) */
                if (if_prot == 2)   dev->type = USB_HID_MOUSE;
                else                dev->type = USB_HID_KBD;
                return 1;
            }
        }
        off += bl;
    }
    return 0;
}

void usb_hid_set_dev(uint8_t slot, const usb_hid_dev_t *dev) {
    if (slot == 0 || slot >= USB_HID_MAX_SLOTS) return;
    usb_hid_devs[slot] = *dev;
    usb_hid_devset[slot] = true;
    usb_hid_key_count[slot] = 0;
    for (int i = 0; i < 6; i++)
        usb_hid_last_keys[slot][i] = 0;
}

static void usb_hid_kbd_report(uint8_t slot, const uint8_t *buf, uint32_t len) {
    if (len < 8) return;

    uint8_t mods = buf[0];
    uint8_t us_mods = 0;
    if (mods & 0x01) us_mods |= KMOD_CTRL;
    if (mods & 0x02) us_mods |= KMOD_SHIFT;
    if (mods & 0x04) us_mods |= KMOD_ALT;
    if (mods & 0x08) us_mods |= KMOD_META;
    if (mods & 0x10) us_mods |= KMOD_CTRL;
    if (mods & 0x20) us_mods |= KMOD_SHIFT;
    if (mods & 0x40) us_mods |= KMOD_ALT;
    if (mods & 0x80) us_mods |= KMOD_META;

    /* Collect the currently-down usage ids (bytes 2..7 of the boot report) */
    uint8_t cur[6];
    uint8_t n = 0;
    for (int i = 0; i < 6; i++) {
        uint8_t k = buf[2 + i];
        if (k) cur[n++] = k;
    }

    /* Releases: previously pressed, gone from this report */
    for (uint8_t i = 0; i < usb_hid_key_count[slot]; i++) {
        bool still = false;
        for (uint8_t j = 0; j < n; j++) {
            if (usb_hid_last_keys[slot][i] == cur[j]) { still = true; break; }
        }
        if (!still)
            keyboard_queue_key(usb_hid_last_keys[slot][i], 0, KEY_EVENT_UP);
    }

    /* Presses: new in this report */
    for (uint8_t j = 0; j < n; j++) {
        bool was = false;
        for (uint8_t i = 0; i < usb_hid_key_count[slot]; i++) {
            if (usb_hid_last_keys[slot][i] == cur[j]) { was = true; break; }
        }
        if (!was) {
            char ascii = keyboard_keycode_to_ascii(cur[j], us_mods);
            keyboard_queue_key(cur[j], ascii, KEY_EVENT_DOWN);
        }
    }

    for (uint8_t i = 0; i < n; i++)
        usb_hid_last_keys[slot][i] = cur[i];
    usb_hid_key_count[slot] = n;

    kprintf("[HID] KBD: mods=0x%X keys=[", mods);
    for (uint8_t i = 0; i < n; i++) {
        kprintf("%u%s", usb_hid_last_keys[slot][i], i + 1 < n ? "," : "");
    }
    kprintf("]\n");
}

static void usb_hid_mouse_report(uint8_t slot, const uint8_t *buf, uint32_t len) {
    (void)slot;
    if (len < 3) return;

    uint8_t buttons = buf[0];
    int8_t dx = (int8_t)buf[1];
    int8_t dy = (int8_t)buf[2];
    int8_t wheel = (len >= 4) ? (int8_t)buf[3] : 0;

    mouse_input(buttons, dx, dy, wheel);
    kprintf("[HID] MS: b=0x%X dx=%d dy=%d wh=%d\n",
            buttons, dx, dy, wheel);
}

void usb_hid_report(uint8_t slot, const uint8_t *buf, uint32_t len, uint8_t epid) {
    if (slot == 0 || slot >= USB_HID_MAX_SLOTS) return;
    if (!usb_hid_devset[slot]) return;
    /* xHC transfer events carry the endpoint INDEX (EP1-IN=3), not the 0x81
     * address from the config descriptor.  addr 0x81 -> idx 3, 0x82 -> 5... */
    uint8_t ea = usb_hid_devs[slot].ep_addr;
    uint8_t eidx = 2 * (ea & 0x0Fu) + ((ea & 0x80) ? 1u : 2u);
    if (epid != eidx) return;

    if (usb_hid_devs[slot].type == USB_HID_KBD)
        usb_hid_kbd_report(slot, buf, len);
    else if (usb_hid_devs[slot].type == USB_HID_MOUSE)
        usb_hid_mouse_report(slot, buf, len);
}