#ifndef RTC_H
#define RTC_H
#include "../include/system.h"

typedef struct datetime_t {
    uint16_t year, month, day;
    uint8_t hour, minute, second;
} datetime_t;

void rtc_init(void);
void rtc_get_time(datetime_t *dt);

#endif
