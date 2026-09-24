#ifndef TIMER_H
#define TIMER_H

#include "../include/types.h"

typedef void (*timer_callback_t)(void*);

struct timer_event {
    uint64_t expire_ms;
    timer_callback_t cb;
    void *arg;
    struct timer_event *next;
};

void timer_init(uint32_t frequency);
uint64_t timer_get_ticks(void);
uint32_t timer_get_seconds(void);
void timer_sleep(uint32_t ms);
uint32_t timer_get_ms(void);
uint64_t timer_get_ms64(void);
int timer_add(uint32_t delay_ms, timer_callback_t cb, void *arg);
void timer_del(struct timer_event *ev);
void timer_tick_process(void);

#endif
