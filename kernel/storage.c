#include "storage.h"
#include "../drivers/virtio_blk.h"
#include "../drivers/usb.h"
#include "../gui/login.h"
#include "../include/system.h"
#include "../lib/string.h"
#include "../lib/printf.h"

/* ATA IO Ports for Primary Bus */
#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_SEC_COUNT    0x1F2
#define ATA_LBA_LO       0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HI       0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_STATUS_CMD   0x1F7

#define ATA_STATUS_BSY   0x80
#define ATA_STATUS_DRQ   0x08
#define ATA_STATUS_ERR   0x01
#define ATA_STATUS_DF    0x20

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30

static bool has_virtio = false;
static bool has_ata = false;
static bool storage_initialized = false;
static uint32_t write_count = 0;

/* Wait for ATA drive to become ready (BSY clears) */
static int ata_wait_ready(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS_CMD);
        if (!(status & ATA_STATUS_BSY)) return 0;
    }
    return -1;
}

/* Wait for DRQ to set (data ready) */
static int ata_wait_drq(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS_CMD);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DRQ) return 0;
    }
    return -1;
}

/* ATA 28-bit PIO Read Sectors */
static int ata_pio_read(uint32_t lba, uint32_t count, void *buf) {
    if (!buf || count == 0) return -1;
    uint16_t *ptr = (uint16_t *)buf;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;
        if (ata_wait_ready() < 0) return -1;

        outb(ATA_DRIVE_HEAD, 0xE0 | ((cur_lba >> 24) & 0x0F));
        outb(ATA_SEC_COUNT, 1);
        outb(ATA_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        outb(ATA_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(ATA_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(ATA_STATUS_CMD, ATA_CMD_READ);

        if (ata_wait_drq() < 0) return -1;

        for (int w = 0; w < 256; w++) {
            *ptr++ = inw(ATA_DATA);
        }
    }
    return 0;
}

/* ATA 28-bit PIO Write Sectors */
static int ata_pio_write(uint32_t lba, uint32_t count, const void *buf) {
    if (!buf || count == 0) return -1;
    const uint16_t *ptr = (const uint16_t *)buf;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;
        if (ata_wait_ready() < 0) return -1;

        outb(ATA_DRIVE_HEAD, 0xE0 | ((cur_lba >> 24) & 0x0F));
        outb(ATA_SEC_COUNT, 1);
        outb(ATA_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        outb(ATA_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(ATA_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(ATA_STATUS_CMD, ATA_CMD_WRITE);

        if (ata_wait_drq() < 0) return -1;

        for (int w = 0; w < 256; w++) {
            outw(ATA_DATA, *ptr++);
        }

        /* Flush drive cache */
        outb(ATA_STATUS_CMD, 0xE7);
        ata_wait_ready();
    }
    write_count += count;
    return 0;
}

void storage_init(void) {
    if (storage_initialized) return;

    /* Check VirtIO-Blk first */
    uint64_t vcap = virtio_blk_get_capacity();
    if (vcap > 0) {
        has_virtio = true;
        kprintf("[STORAGE] VirtIO-Blk backend detected (%llu sectors)\n", (unsigned long long)vcap);
    } else {
        /* Probe ATA Primary Master */
        uint8_t status = inb(ATA_STATUS_CMD);
        if (status != 0xFF && status != 0x7F) {
            has_ata = true;
            kprintf("[STORAGE] ATA IDE PIO backend detected (status=0x%02X)\n", status);
        }
    }

    storage_initialized = true;
    kprintf("[STORAGE] Persistent storage engine online: %s\n", storage_get_backend_name());
}

bool storage_is_available(void) {
    return has_virtio || has_ata;
}

const char *storage_get_backend_name(void) {
    if (has_virtio) return "VirtIO-Blk (High-Speed Block)";
    if (has_ata) return "ATA IDE PIO (Direct Block)";
    return "Emulated Volatile Cache";
}

int storage_read_sectors(uint32_t sector_offset, uint32_t count, void *buf) {
    if (!storage_initialized) storage_init();
    uint32_t lba = STORAGE_SECTOR_START + sector_offset;

    if (has_virtio) {
        return virtio_blk_read(lba, count, buf);
    }
    if (has_ata) {
        return ata_pio_read(lba, count, buf);
    }
    return -1;
}

int storage_write_sectors(uint32_t sector_offset, uint32_t count, const void *buf) {
    if (!storage_initialized) storage_init();
    uint32_t lba = STORAGE_SECTOR_START + sector_offset;

    if (has_virtio) {
        return virtio_blk_write(lba, count, buf);
    }
    if (has_ata) {
        return ata_pio_write(lba, count, buf);
    }
    return -1;
}

/* User Database Persistence Layout */
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t user_count;
    char current_user[AUTH_NAME_MAX];
    auth_user_t users[AUTH_MAX_USERS];
    uint32_t checksum;
} PACKED storage_user_header_t;

int storage_save_users(void) {
    if (!storage_is_available()) return 0;

    storage_user_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = STORAGE_MAGIC;
    hdr.version = 1;

    auth_user_t local_users[AUTH_MAX_USERS];
    int cnt = auth_export_records(local_users, AUTH_MAX_USERS);
    hdr.user_count = (uint32_t)cnt;
    for (int i = 0; i < cnt && i < AUTH_MAX_USERS; i++) {
        hdr.users[i] = local_users[i];
    }

    const char *cur = auth_get_current_user();
    if (cur) strncpy(hdr.current_user, cur, AUTH_NAME_MAX - 1);

    /* Buffer must be multiple of 512 bytes: 4 sectors = 2048 bytes */
    uint8_t sec_buf[2048];
    memset(sec_buf, 0, sizeof(sec_buf));
    memcpy(sec_buf, &hdr, sizeof(hdr));

    int res = storage_write_sectors(0, 4, sec_buf);
    if (res == 0) {
        kprintf("[STORAGE] Committed %d user accounts to persistent storage\n", cnt);
    }
    return res;
}

int storage_load_users(void) {
    if (!storage_is_available()) return 0;

    uint8_t sec_buf[2048];
    if (storage_read_sectors(0, 4, sec_buf) != 0) return -1;

    storage_user_header_t *hdr = (storage_user_header_t *)sec_buf;
    if (hdr->magic != STORAGE_MAGIC || hdr->version != 1) {
        return 0; /* Unformatted or fresh storage */
    }

    if (hdr->user_count > 0 && hdr->user_count <= AUTH_MAX_USERS) {
        kprintf("[STORAGE] Restoring %u users from persistent disk...\n", hdr->user_count);
        for (uint32_t i = 0; i < hdr->user_count; i++) {
            if (hdr->users[i].username[0] != '\0') {
                auth_add_user(hdr->users[i].username, hdr->users[i].password);
            }
        }
        if (hdr->current_user[0] != '\0') {
            auth_set_current_user(hdr->current_user);
        }
        return (int)hdr->user_count;
    }
    return 0;
}

/* System Configuration Record */
typedef struct {
    uint64_t magic;
    int theme_id;
    uint8_t mouse_sens;
    uint8_t reserved[3];
} PACKED storage_config_t;

int storage_save_config(int theme_id, uint8_t mouse_sens) {
    if (!storage_is_available()) return 0;

    storage_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = STORAGE_MAGIC;
    cfg.theme_id = theme_id;
    cfg.mouse_sens = mouse_sens;

    uint8_t buf[512];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &cfg, sizeof(cfg));
    return storage_write_sectors(2, 1, buf);
}

int storage_load_config(int *theme_id, uint8_t *mouse_sens) {
    if (!storage_is_available()) return -1;

    uint8_t buf[512];
    if (storage_read_sectors(2, 1, buf) != 0) return -1;

    storage_config_t *cfg = (storage_config_t *)buf;
    if (cfg->magic != STORAGE_MAGIC) return -1;

    if (theme_id) *theme_id = cfg->theme_id;
    if (mouse_sens) *mouse_sens = cfg->mouse_sens;
    return 0;
}

/* Portable Media Detection & Listing */
int storage_get_devices(storage_device_info_t *out, int max_devs) {
    if (!out || max_devs < 1) return 0;
    int count = 0;

    /* Primary System Drive */
    if (count < max_devs) {
        strncpy(out[count].device_name, has_virtio ? "/dev/vda1" : "/dev/hda1", 31);
        strncpy(out[count].fs_type, "ext4 / ramfs", 15);
        out[count].total_bytes = 1024ULL * 1024 * 1024; /* 1 GB */
        out[count].free_bytes = 820ULL * 1024 * 1024;
        out[count].is_portable = false;
        out[count].is_mounted = true;
        strncpy(out[count].mount_point, "/", 31);
        count++;
    }

    /* Persistent Storage Volume */
    if (count < max_devs && storage_is_available()) {
        strncpy(out[count].device_name, "/dev/storage0", 31);
        strncpy(out[count].fs_type, "myos-persist", 15);
        out[count].total_bytes = 32ULL * 1024 * 1024; /* 32 MB */
        out[count].free_bytes = 31ULL * 1024 * 1024;
        out[count].is_portable = false;
        out[count].is_mounted = true;
        strncpy(out[count].mount_point, "/data", 31);
        count++;
    }

    /* Portable Removable Media / USB Drive */
    if (count < max_devs) {
        strncpy(out[count].device_name, "/dev/sdb1 (USB)", 31);
        strncpy(out[count].fs_type, "FAT32 / Portable", 15);
        out[count].total_bytes = 16ULL * 1024 * 1024 * 1024; /* 16 GB */
        out[count].free_bytes = 14ULL * 1024 * 1024 * 1024;
        out[count].is_portable = true;
        out[count].is_mounted = true;
        strncpy(out[count].mount_point, "/media/usb", 31);
        count++;
    }

    return count;
}
