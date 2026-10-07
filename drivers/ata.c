#include "ata.h"
#include "../include/system.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "driver.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* ATA IO Registers */
#define ATA_REG_DATA       0
#define ATA_REG_ERROR      1
#define ATA_REG_FEATURES   1
#define ATA_REG_SECCOUNT   2
#define ATA_REG_LBA_LO     3
#define ATA_REG_LBA_MID    4
#define ATA_REG_LBA_HI     5
#define ATA_REG_DRIVE      6
#define ATA_REG_STATUS     7
#define ATA_REG_COMMAND    7

/* Status register bits */
#define ATA_SR_BSY         0x80    /* Busy */
#define ATA_SR_DRDY        0x40    /* Drive ready */
#define ATA_SR_DF          0x20    /* Drive fault */
#define ATA_SR_DSC         0x10    /* Drive seek complete */
#define ATA_SR_DRQ         0x08    /* Data request ready */
#define ATA_SR_CORR        0x04    /* Corrected data */
#define ATA_SR_IDX         0x02    /* Index */
#define ATA_SR_ERR         0x01    /* Error */

/* Commands */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_FLUSH_CACHE     0xE7
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

static ata_device_t g_ata_devs[4];
ata_device_t *ata_devices[4] = { NULL, NULL, NULL, NULL };

static void ata_delay_400ns(uint16_t ctrl) {
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);
}

static int ata_wait_bsy(uint16_t io, uint16_t ctrl) {
    (void)ctrl;
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(io + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return 0;
    }
    return -1;
}

static int ata_wait_drq(uint16_t io, uint16_t ctrl) {
    (void)ctrl;
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(io + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (status & ATA_SR_DRQ) return 0;
    }
    return -1;
}

static void ata_format_string(char *dst, const uint8_t *src, int len) {
    int out_idx = 0;
    for (int i = 0; i < len; i += 2) {
        dst[out_idx++] = src[i + 1];
        dst[out_idx++] = src[i];
    }
    while (out_idx > 0 && dst[out_idx - 1] == ' ') out_idx--;
    dst[out_idx] = '\0';
}

static int ata_identify(ata_device_t *dev) {
    outb(dev->io_base + ATA_REG_DRIVE, dev->drive);
    ata_delay_400ns(dev->ctrl_base);

    outb(dev->io_base + ATA_REG_SECCOUNT, 0);
    outb(dev->io_base + ATA_REG_LBA_LO, 0);
    outb(dev->io_base + ATA_REG_LBA_MID, 0);
    outb(dev->io_base + ATA_REG_LBA_HI, 0);

    outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay_400ns(dev->ctrl_base);

    uint8_t status = inb(dev->io_base + ATA_REG_STATUS);
    if (status == 0) {
        return -1; /* Device does not exist */
    }

    if (ata_wait_bsy(dev->io_base, dev->ctrl_base) < 0) {
        return -1;
    }

    uint8_t mid = inb(dev->io_base + ATA_REG_LBA_MID);
    uint8_t hi  = inb(dev->io_base + ATA_REG_LBA_HI);
    if (mid == 0x14 && hi == 0xEB) {
        dev->is_atapi = true;
        dev->present = true;
        strcpy(dev->model, "ATAPI CD-ROM/Optical");
        return 0;
    }

    if (ata_wait_drq(dev->io_base, dev->ctrl_base) < 0) {
        return -1;
    }

    uint16_t buffer[256];
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(dev->io_base + ATA_REG_DATA);
    }

    uint8_t *b = (uint8_t *)buffer;
    ata_format_string(dev->serial, &b[20], 20);
    ata_format_string(dev->model, &b[54], 40);

    dev->lba48_supported = (buffer[83] & (1 << 10)) != 0;
    dev->sectors = (uint32_t)buffer[60] | ((uint32_t)buffer[61] << 16);
    dev->present = true;

    return 0;
}

void ata_init(void) {
    uint16_t bases[2] = { 0x1F0, 0x170 };
    uint16_t ctrls[2] = { 0x3F6, 0x376 };
    uint8_t drives[2] = { 0xA0, 0xB0 };

    int found = 0;
    for (int ch = 0; ch < 2; ch++) {
        for (int drv = 0; drv < 2; drv++) {
            int idx = ch * 2 + drv;
            ata_device_t *dev = &g_ata_devs[idx];
            memset(dev, 0, sizeof(ata_device_t));
            dev->io_base = bases[ch];
            dev->ctrl_base = ctrls[ch];
            dev->drive = drives[drv];

            if (ata_identify(dev) == 0 && dev->present) {
                ata_devices[idx] = dev;
                found++;
                kprintf("[ATA] Disk %d (%s %s): %s (%u sectors, %u MB)\n",
                        idx, ch == 0 ? "Primary" : "Secondary",
                        drv == 0 ? "Master" : "Slave",
                        dev->model, dev->sectors,
                        (dev->sectors * 512) / (1024 * 1024));
            }
        }
    }

    char desc[64];
    snprintf(desc, sizeof(desc), "ATA IDE PIO driver (%d drive%s active)",
             found, found == 1 ? "" : "s");
    driver_register("ata-ide", "storage", found > 0 ? "ready" : "none", desc);
}

int ata_read_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, void *buf) {
    if (!dev || !dev->present || !buf || count == 0) return -1;
    uint16_t *ptr = (uint16_t *)buf;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;

        if (ata_wait_bsy(dev->io_base, dev->ctrl_base) < 0) return -1;

        uint8_t drive_sel = (dev->drive == 0xB0 ? 0xF0 : 0xE0) | ((cur_lba >> 24) & 0x0F);
        outb(dev->io_base + ATA_REG_DRIVE, drive_sel);
        ata_delay_400ns(dev->ctrl_base);

        outb(dev->io_base + ATA_REG_SECCOUNT, 1);
        outb(dev->io_base + ATA_REG_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        outb(dev->io_base + ATA_REG_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(dev->io_base + ATA_REG_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (ata_wait_drq(dev->io_base, dev->ctrl_base) < 0) return -1;

        for (int w = 0; w < 256; w++) {
            *ptr++ = inw(dev->io_base + ATA_REG_DATA);
        }
    }
    return 0;
}

int ata_write_sectors(ata_device_t *dev, uint32_t lba, uint32_t count, const void *buf) {
    if (!dev || !dev->present || !buf || count == 0) return -1;
    const uint16_t *ptr = (const uint16_t *)buf;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;

        if (ata_wait_bsy(dev->io_base, dev->ctrl_base) < 0) return -1;

        uint8_t drive_sel = (dev->drive == 0xB0 ? 0xF0 : 0xE0) | ((cur_lba >> 24) & 0x0F);
        outb(dev->io_base + ATA_REG_DRIVE, drive_sel);
        ata_delay_400ns(dev->ctrl_base);

        outb(dev->io_base + ATA_REG_SECCOUNT, 1);
        outb(dev->io_base + ATA_REG_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        outb(dev->io_base + ATA_REG_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(dev->io_base + ATA_REG_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        if (ata_wait_drq(dev->io_base, dev->ctrl_base) < 0) return -1;

        for (int w = 0; w < 256; w++) {
            outw(dev->io_base + ATA_REG_DATA, *ptr++);
        }

        outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
        ata_wait_bsy(dev->io_base, dev->ctrl_base);
    }
    return 0;
}

ata_device_t *ata_get_device(int idx) {
    if (idx < 0 || idx >= 4) return NULL;
    return ata_devices[idx];
}
