#ifndef KERNEL_STORAGE_H
#define KERNEL_STORAGE_H

#include "../include/types.h"

#define STORAGE_MAGIC 0x4D594F535F53544FULL /* "MYOS_STO" */
#define STORAGE_SECTOR_START 20480          /* 10MB offset on disk */

typedef struct {
    char device_name[32];
    char fs_type[16];
    uint64_t total_bytes;
    uint64_t free_bytes;
    bool is_portable;
    bool is_mounted;
    char mount_point[32];
} storage_device_info_t;

void storage_init(void);
bool storage_is_available(void);
const char *storage_get_backend_name(void);

/* Low-level sector read/write */
int storage_read_sectors(uint32_t sector_offset, uint32_t count, void *buf);
int storage_write_sectors(uint32_t sector_offset, uint32_t count, const void *buf);

/* High-level persistence */
int storage_save_users(void);
int storage_load_users(void);
int storage_save_config(int theme_id, uint8_t mouse_sens);
int storage_load_config(int *theme_id, uint8_t *mouse_sens);

/* Portable media detection */
int storage_get_devices(storage_device_info_t *out, int max_devs);

#endif
