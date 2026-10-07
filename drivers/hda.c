#include "hda.h"
#include "pci.h"
#include "driver.h"
#include "../lib/printf.h"

static hda_t hda;

int hda_init(void) {
    pci_device_t dev;
    /* PCI Class 0x04 Subclass 0x03 is Intel High Definition Audio */
    if (pci_find_by_class(0x04, 0x03, &dev)) {
        hda.mmio_base = dev.bars[0];
        hda.codec_count = 1;
        pci_enable_bus_master(dev.loc.bus, dev.loc.slot, dev.loc.func);
        kprintf("[HDA] Intel High Definition Audio controller detected at %02x:%02x.%x (BAR0=0x%08x)\n",
                dev.loc.bus, dev.loc.slot, dev.loc.func, (uint32_t)dev.bars[0]);
        driver_register("hda", "audio", "ready", "Intel High Definition Audio Controller");
        return 0;
    }

    hda.mmio_base = 0;
    hda.codec_count = 0;
    driver_register("hda", "audio", "none", "Intel HD Audio not present (AC'97/Speaker active)");
    return 0;
}

int hda_play_stream(int stream_id, const void *buf, size_t len) {
    (void)stream_id; (void)buf; (void)len;
    return -1;
}
