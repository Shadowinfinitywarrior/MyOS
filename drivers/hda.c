#include "hda.h"
#include <lib/printf.h>

static hda_t hda;

int hda_init(void) {
    hda.mmio_base = 0;
    hda.codec_count = 0;
    kprintf("[hda] init stub\n");
    return 0;
}

int hda_play_stream(int stream_id, const void *buf, size_t len) {
    return -1;
}
