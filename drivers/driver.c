#include "driver.h"
#include "../lib/string.h"

/* Fixed table: no allocation, so a driver can register even very early in
 * boot before the heap is fully warmed up. */
static myos_driver_info_t g_drivers[DRIVER_MAX];
static int g_driver_count = 0;

/* Copy a string into a fixed field, always leaving a terminator. */
static void copy_field(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (src) {
        for (; i + 1 < cap && src[i]; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

void driver_register(const char *name, const char *category,
                     const char *status, const char *description) {
    if (g_driver_count >= DRIVER_MAX) return;
    myos_driver_info_t *d = &g_drivers[g_driver_count++];
    copy_field(d->name, sizeof(d->name), name);
    copy_field(d->category, sizeof(d->category), category);
    copy_field(d->status, sizeof(d->status), status);
    copy_field(d->description, sizeof(d->description), description);
}

int driver_count(void) {
    return g_driver_count;
}

const myos_driver_info_t *driver_get(int index) {
    if (index < 0 || index >= g_driver_count) return NULL;
    return &g_drivers[index];
}

/*
 * Drivers that are always part of this kernel and have no separate probe:
 * they are wired into the boot path directly. Registering them here means the
 * "drivers" command shows a complete picture, not just the plug-in devices.
 */
void driver_seed_core(void) {
    driver_register("screen", "display", "ready",
                    "VGA text / framebuffer console output");
    driver_register("keyboard", "input", "ready",
                    "PS/2 keyboard with repeat and modifier support");
    driver_register("mouse", "input", "ready",
                    "PS/2 mouse and touchpad pointer input");
    driver_register("serial", "comm", "ready",
                    "COM1 serial port plus QEMU debug port");
    driver_register("pci", "bus", "ready",
                    "PCI configuration-space access and device scan");
    driver_register("rtc", "time", "ready",
                    "Real-time clock: wall-clock date and time");
    driver_register("timer", "time", "ready",
                    "APIC/PIT timer at 1000 ticks per second");
    driver_register("speaker", "audio", "ready",
                    "PC speaker tones and beeps");
    driver_register("ramfs", "storage", "ready",
                    "In-memory read/write filesystem for /");
    driver_register("devfs", "storage", "ready",
                    "Device files under /dev");
    driver_register("virtio-blk", "storage", "ready",
                    "Virtio block disk (QEMU data.img)");
    driver_register("virtio-net", "net", "ready",
                    "Virtio network card");
    driver_register("console", "display", "ready",
                    "Input line and prompt management");
}
