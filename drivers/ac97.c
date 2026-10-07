#include "ac97.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static bool playing = false;

void ac97_init(uint16_t nambar, uint16_t nabmbar, uint8_t irq) {
    (void)nambar; (void)nabmbar; (void)irq;
    kprintf("[AC97] Initialized\n");
}

void ac97_probe(void) {
    kprintf("[AC97] Probing... not found (stub)\n");
}

void ac97_play(const uint8_t *samples, uint32_t num_samples, uint32_t sample_rate) {
    (void)samples; (void)num_samples; (void)sample_rate;
    playing = true;
}

void ac97_stop(void) { playing = false; }
bool ac97_is_playing(void) { return playing; }
void ac97_beep(uint32_t freq, uint32_t duration_ms) {
    kprintf("[AC97] Beep %uHz %ums\n", freq, duration_ms);
    timer_sleep(duration_ms);
}
