#ifndef USB_HID_H
#define USB_HID_H
#include "../include/types.h"

/* HID device classes we understand */
#define USB_HID_KBD      1   /* Boot HID keyboard */
#define USB_HID_MOUSE    2   /* Boot HID mouse */

typedef struct {
    uint8_t  type;          /* USB_HID_KBD or USB_HID_MOUSE */
    uint8_t  ep_addr;       /* endpoint address (0x81 = EP1 IN) */
    uint16_t max_packet;    /* wMaxPacketSize from the endpoint descriptor */
    uint8_t  interval;      /* bInterval from the endpoint descriptor */
} usb_hid_dev_t;

/* Generic init/poll hooks (kept for API compatibility) */
int usb_hid_init(void);
int usb_hid_poll(void);

/* Parse a configuration descriptor image and locate the boot HID interface
 * (class 3, subclass 1 = keyboard, 2 = mouse) with its interrupt IN endpoint.
 * Returns 1 and fills *dev on success, 0 otherwise. */
int usb_hid_parse_cfgdesc(const uint8_t *cfg, uint16_t len, usb_hid_dev_t *dev);

/* Record an enumerated HID device so report dispatch knows how to decode it. */
void usb_hid_set_dev(uint8_t slot, const usb_hid_dev_t *dev);

/* Process a completed IN transfer on the HID endpoint for the given slot.
 * Decodes the boot protocol report and feeds the shared mouse/keyboard paths. */
void usb_hid_report(uint8_t slot, const uint8_t *buf, uint32_t len, uint8_t epid);

#endif