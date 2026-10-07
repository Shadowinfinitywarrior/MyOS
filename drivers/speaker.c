#include "speaker.h"
#include "../include/system.h"
#include "../kernel/timer.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define PIT_FREQ 1193182

void speaker_init(void) {
    kprintf("[SPEAKER] PC speaker initialized\n");
}

static void speaker_on(uint32_t frequency) {
    uint32_t divisor = PIT_FREQ / frequency;

    /* Set PIT channel 2 to square wave */
    outb(0x43, 0xB6);
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);

    /* Enable speaker (bits 0-1 of port 0x61) */
    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3))
        outb(0x61, tmp | 3);
}

void speaker_off(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

static volatile uint64_t speaker_off_ms = 0;
static volatile bool     speaker_active = false;

void speaker_poll(void) {
    if (speaker_active) {
        uint64_t now = timer_get_ms64();
        if (now >= speaker_off_ms) {
            speaker_off();
            speaker_active = false;
        }
    }
}

void speaker_beep_async(uint32_t frequency, uint32_t duration_ms) {
    if (frequency < 20 || frequency > 20000) return;
    speaker_on(frequency);
    speaker_off_ms = timer_get_ms64() + duration_ms;
    speaker_active = true;
}

void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_beep_async(frequency, duration_ms);
}

void speaker_play_note(const char *note, uint32_t duration_ms) {
    uint32_t freq = 0;

    /* Note frequencies (4th octave) */
    if (strcmp(note, "C4") == 0)  freq = 262;
    if (strcmp(note, "D4") == 0)  freq = 294;
    if (strcmp(note, "E4") == 0)  freq = 330;
    if (strcmp(note, "F4") == 0)  freq = 349;
    if (strcmp(note, "G4") == 0)  freq = 392;
    if (strcmp(note, "A4") == 0)  freq = 440;
    if (strcmp(note, "B4") == 0)  freq = 494;
    if (strcmp(note, "C5") == 0)  freq = 523;

    if (freq)
        speaker_beep_async(freq, duration_ms);
}

void speaker_play_click(void) {
    speaker_beep_async(880, 8);
}

void speaker_play_chime(void) {
    speaker_beep_async(659, 45);
}

