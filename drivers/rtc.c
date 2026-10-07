#include "rtc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"


void rtc_init(void) {
    /* Enable NMI and read status B */
    outb(0x70, 0x8B);
    uint8_t prev = inb(0x71);
    outb(0x70, 0x8B);
    outb(0x71, prev | 0x02); /* 24 hour format */
}

static uint8_t rtc_read_reg(uint8_t reg) {
    outb(0x70, reg & 0x7F);
    return inb(0x71);
}

static int rtc_is_updating(void) {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

void rtc_get_time(datetime_t *dt) {
    if (!dt) return;

    /* Wait while RTC is updating to avoid tearing */
    int timeout = 10000;
    while (rtc_is_updating() && --timeout);

    uint8_t sec = rtc_read_reg(0x00);
    uint8_t min = rtc_read_reg(0x02);
    uint8_t hour = rtc_read_reg(0x04);
    uint8_t day = rtc_read_reg(0x07);
    uint8_t month = rtc_read_reg(0x08);
    uint8_t year = rtc_read_reg(0x09);
    uint8_t register_b = rtc_read_reg(0x0B);

    /* Convert BCD to binary if bit 2 of Register B is clear */
    if (!(register_b & 0x04)) {
        sec = ((sec >> 4) * 10) + (sec & 0x0F);
        min = ((min >> 4) * 10) + (min & 0x0F);
        hour = ((((hour & 0x3F) >> 4) * 10) + (hour & 0x0F)) | (hour & 0x80);
        day = ((day >> 4) * 10) + (day & 0x0F);
        month = ((month >> 4) * 10) + (month & 0x0F);
        year = ((year >> 4) * 10) + (year & 0x0F);
    }

    /* Convert 12h to 24h if bit 1 of Register B is clear and 12-hour mode */
    if (!(register_b & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    dt->second = sec;
    dt->minute = min;
    dt->hour = hour;
    dt->day = day;
    dt->month = month;
    dt->year = 2000 + year;
}
