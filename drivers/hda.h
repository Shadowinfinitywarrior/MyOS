#pragma once
#include <stdint.h>

typedef struct {
    uint64_t mmio_base;
    uint32_t codec_count;
} hda_t;

int hda_init(void);
int hda_play_stream(int stream_id, const void *buf, size_t len);
