#include "rtc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"


void rtc_init(void) {
    /* ensure RTC is in BCD mode? leave as is */
}

static uint8_t rtc_read_reg(uint8_t reg) {
    outb(0x70, reg & 0x7F);
    return inb(0x71);
}

void rtc_get_time(datetime_t *dt) {
    if (!dt) return;
    uint8_t sec = rtc_read_reg(0x00);
    uint8_t min = rtc_read_reg(0x02);
    uint8_t hour = rtc_read_reg(0x04);
    uint8_t day = rtc_read_reg(0x07);
    uint8_t month = rtc_read_reg(0x08);
    uint8_t year = rtc_read_reg(0x09);
    /* CMOS returns BCD; convert */
    #define BCD_TO_BIN(v) (((v) >> 4) * 10 + ((v) & 0x0F))
    sec = BCD_TO_BIN(sec);
    min = BCD_TO_BIN(min);
    hour = BCD_TO_BIN(hour & 0x3F);
    day = BCD_TO_BIN(day);
    month = BCD_TO_BIN(month);
    year = BCD_TO_BIN(year);
    dt->second = sec;
    dt->minute = min;
    dt->hour = hour;
    dt->day = day;
    dt->month = month;
    dt->year = 2000 + year;
}
