#include "driver.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/heap.h"

static myos_driver_info_t g_drivers[DRIVER_MAX];
static int g_driver_count = 0;

static bus_t *bus_head = NULL;
static spinlock_t driver_model_lock = SPINLOCK_INIT;

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

int bus_register(bus_t *bus) {
    if (!bus) return -1;
    uint64_t flags = spin_lock_irqsave(&driver_model_lock);
    bus->next = bus_head;
    bus_head = bus;
    spin_unlock_irqrestore(&driver_model_lock, flags);
    kprintf("[BUS] Registered bus subsystem: %s\n", bus->name);
    return 0;
}

int driver_module_register(driver_obj_t *drv) {
    if (!drv) return -1;
    uint64_t flags = spin_lock_irqsave(&driver_model_lock);
    for (bus_t *b = bus_head; b; b = b->next) {
        if (b->bus_type == drv->bus_type) {
            drv->next = b->drivers;
            b->drivers = drv;
            /* Match against already registered devices */
            for (device_t *dev = b->devices; dev; dev = dev->next) {
                if (b->match && b->match(dev, drv)) {
                    if (drv->probe) drv->probe(dev);
                }
            }
            break;
        }
    }
    spin_unlock_irqrestore(&driver_model_lock, flags);
    kprintf("[DRIVER] Registered driver module: %s\n", drv->name);
    return 0;
}

int device_register(device_t *dev) {
    if (!dev) return -1;
    uint64_t flags = spin_lock_irqsave(&driver_model_lock);
    for (bus_t *b = bus_head; b; b = b->next) {
        if (b->bus_type == dev->bus_type) {
            dev->next = b->devices;
            b->devices = dev;
            /* Match against existing drivers */
            for (driver_obj_t *drv = b->drivers; drv; drv = drv->next) {
                if (b->match && b->match(dev, drv)) {
                    if (drv->probe) drv->probe(dev);
                    break;
                }
            }
            break;
        }
    }
    spin_unlock_irqrestore(&driver_model_lock, flags);
    return 0;
}

void device_hotplug(device_t *dev, int action) {
    if (!dev) return;
    if (action == DEVICE_HOTPLUG_ADD) {
        kprintf("[HOTPLUG] Device added: %s\n", dev->name);
        device_register(dev);
    } else if (action == DEVICE_HOTPLUG_REMOVE) {
        kprintf("[HOTPLUG] Device removed: %s\n", dev->name);
    }
}

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
}
