#ifndef DRIVER_H
#define DRIVER_H

#include "../include/types.h"
#include "../include/myosinfo.h"
#include "../include/spinlock.h"

#define DRIVER_MAX 64

#define BUS_PLATFORM 1
#define BUS_PCI      2
#define BUS_USB      3
#define BUS_VIRTIO   4

#define DEVICE_HOTPLUG_ADD    1
#define DEVICE_HOTPLUG_REMOVE 2

typedef struct device {
    char name[64];
    uint32_t bus_type;     /* BUS_PCI, BUS_USB, BUS_VIRTIO, BUS_PLATFORM */
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t class_code;
    void *mmio_base;
    uint32_t irq;
    struct device *parent;
    struct device *next;
    void *driver_data;
} device_t;

typedef struct driver_obj {
    char name[64];
    uint32_t bus_type;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t class_code;
    int (*probe)(device_t *dev);
    int (*remove)(device_t *dev);
    int (*suspend)(device_t *dev);
    int (*resume)(device_t *dev);
    struct driver_obj *next;
} driver_obj_t;

typedef struct bus {
    char name[32];
    uint32_t bus_type;
    int (*enumerate)(void);
    int (*match)(device_t *dev, driver_obj_t *drv);
    device_t *devices;
    driver_obj_t *drivers;
    struct bus *next;
} bus_t;

/* Legacy plain-text driver registry */
void driver_register(const char *name, const char *category,
                     const char *status, const char *description);
int  driver_count(void);
const myos_driver_info_t *driver_get(int index);
void driver_seed_core(void);

/* Loadable Driver Model */
int bus_register(bus_t *bus);
int driver_module_register(driver_obj_t *drv);
int device_register(device_t *dev);
void device_hotplug(device_t *dev, int action);

#endif
