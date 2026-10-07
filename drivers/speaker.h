#ifndef SPEAKER_H
#define SPEAKER_H

#include "../include/types.h"

void speaker_init(void);
void speaker_beep(uint32_t frequency, uint32_t duration_ms);
void speaker_play_note(const char *note, uint32_t duration_ms);
void speaker_play_chime(void);
void speaker_play_click(void);
void speaker_off(void);

#endif

