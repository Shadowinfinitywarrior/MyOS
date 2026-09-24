#ifndef AC97_H
#define AC97_H

#include "../include/types.h"

void ac97_init(uint16_t nambar, uint16_t nabmbar, uint8_t irq);
void ac97_play(const uint8_t *samples, uint32_t num_samples, uint32_t sample_rate);
void ac97_stop(void);
void ac97_beep(uint32_t freq, uint32_t duration_ms);
bool ac97_is_playing(void);

#endif
