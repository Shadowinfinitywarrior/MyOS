#include "ac97.h"
#include "pci.h"
#include "speaker.h"
#include "driver.h"
#include "../lib/printf.h"
#include "../kernel/timer.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* AC'97 Mixer Registers */
#define AC97_MIXER_RESET          0x00
#define AC97_MIXER_MASTER_VOL     0x02
#define AC97_MIXER_AUX_VOL        0x04
#define AC97_MIXER_MONO_VOL       0x06
#define AC97_MIXER_MIC_VOL        0x0E
#define AC97_MIXER_LINEIN_VOL     0x10
#define AC97_MIXER_CD_VOL         0x12
#define AC97_MIXER_PCM_VOL        0x18
#define AC97_MIXER_RECORD_SELECT  0x1A
#define AC97_MIXER_RECORD_GAIN    0x1C
#define AC97_MIXER_GENERAL_PURP   0x20
#define AC97_MIXER_EXT_AUDIO_ID   0x28
#define AC97_MIXER_EXT_AUDIO_STAT 0x2A
#define AC97_MIXER_FRONT_RATE     0x2C

/* Bus Master Registers (offsets from NABMBAR) */
#define AC97_BM_PO_BDBAR          0x10  /* PCM Out Buffer Descriptor Base Address */
#define AC97_BM_PO_CIV            0x14  /* Current Index Value */
#define AC97_BM_PO_LVI            0x15  /* Last Valid Index */
#define AC97_BM_PO_SR             0x16  /* Status Register */
#define AC97_BM_PO_PICB           0x18  /* Position in Current Buffer */
#define AC97_BM_PO_PIV            0x1A  /* Prefetched Index Value */
#define AC97_BM_PO_CR             0x1B  /* Control Register */

static uint16_t ac97_nambar = 0;
static uint16_t ac97_nabmbar = 0;
static uint8_t  ac97_irq = 0;
static bool     ac97_present = false;
static bool     playing = false;

void ac97_init(uint16_t nambar, uint16_t nabmbar, uint8_t irq) {
    ac97_nambar = nambar;
    ac97_nabmbar = nabmbar;
    ac97_irq = irq;
    ac97_present = true;

    /* Cold reset the AC'97 codec */
    outw(ac97_nambar + AC97_MIXER_RESET, 0x0001);

    /* Unmute and set master volume to 0dB attenuation (max output) */
    outw(ac97_nambar + AC97_MIXER_MASTER_VOL, 0x0000);

    /* Unmute and set PCM output volume to 0dB */
    outw(ac97_nambar + AC97_MIXER_PCM_VOL, 0x0000);

    /* Set default DAC sample rate to 48000 Hz if variable rate is supported */
    outw(ac97_nambar + AC97_MIXER_FRONT_RATE, 48000);

    /* Reset Bus Master PCM Out registers */
    outb(ac97_nabmbar + AC97_BM_PO_CR, 0x02); /* Reset */

    kprintf("[AC97] Hardware codec initialized (NAMBAR=0x%04x, NABMBAR=0x%04x, IRQ=%u)\n",
            ac97_nambar, ac97_nabmbar, ac97_irq);
    driver_register("ac97", "audio", "ready", "Intel 82801 AC'97 Audio Controller");
}

void ac97_probe(void) {
    pci_device_t dev;

    /* Probe for Intel 82801 AA/AB/BA AC'97 audio controller (0x8086:0x2415) */
    pci_dev_info_t info;
    if (pci_find_device(0x8086, 0x2415, &info)) {
        uint32_t bar0 = pci_get_bar(info.loc.bus, info.loc.slot, info.loc.func, 0);
        uint32_t bar1 = pci_get_bar(info.loc.bus, info.loc.slot, info.loc.func, 1);
        pci_enable_bus_master(info.loc.bus, info.loc.slot, info.loc.func);
        ac97_init((uint16_t)bar0, (uint16_t)bar1, info.irq_line);
        return;
    }

    /* Probe by PCI Multimedia Audio Controller class 0x04 subclass 0x01 */
    if (pci_find_by_class(0x04, 0x01, &dev)) {
        uint32_t bar0 = dev.bars[0];
        uint32_t bar1 = dev.bars[1];
        pci_enable_bus_master(dev.loc.bus, dev.loc.slot, dev.loc.func);
        ac97_init((uint16_t)bar0, (uint16_t)bar1, dev.irq_line);
        return;
    }

    ac97_present = false;
    kprintf("[AC97] No AC'97 hardware controller found, fallback to PC Speaker audio engine\n");
    driver_register("ac97", "audio", "fallback", "PC Speaker sound synthesis (PIT Channel 2)");
}

void ac97_play(const uint8_t *samples, uint32_t num_samples, uint32_t sample_rate) {
    (void)samples; (void)num_samples; (void)sample_rate;
    playing = true;
}

void ac97_stop(void) {
    playing = false;
    speaker_off();
}

bool ac97_is_playing(void) {
    return playing;
}

bool ac97_is_available(void) {
    return ac97_present;
}

void ac97_beep(uint32_t freq, uint32_t duration_ms) {
    sound_play_tone(freq, duration_ms);
}

void sound_play_tone(uint32_t freq, uint32_t duration_ms) {
    if (freq == 0 || duration_ms == 0) return;
    playing = true;
    speaker_beep_async(freq, duration_ms);
    playing = false;
}

void sound_play_click(void) {
    speaker_play_click();
}

void sound_play_chime(void) {
    speaker_play_chime();
}
