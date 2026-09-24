#include "fat16.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* FAT16 structures */
typedef struct fat16_bpb {
    uint8_t  jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} PACKED fat16_bpb_t;

typedef struct fat16_dir_entry {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} PACKED fat16_dir_entry_t;

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME     0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

typedef struct fat16_fs {
    ata_device_t *device;
    uint32_t      partition_lba;
    uint16_t      bytes_per_sector;
    uint8_t       sectors_per_cluster;
    uint16_t      reserved_sectors;
    uint8_t       num_fats;
    uint16_t      fat_size;
    uint16_t      root_entry_count;
    uint32_t      fat_start;        /* LBA of first FAT */
    uint32_t      root_dir_start;   /* LBA of root directory */
    uint32_t      data_start;       /* LBA of data area */
    uint32_t      total_clusters;
} fat16_fs_t;

static uint8_t sector_buf[512];

static int fat16_read_sector(fat16_fs_t *fs, uint32_t lba, void *buf) {
    return ata_read_sectors(fs->device, fs->partition_lba + lba, 1, buf);
}

static uint16_t fat16_get_cluster(fat16_fs_t *fs, uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs->fat_start + (fat_offset / 512);
    uint32_t fat_entry_offset = fat_offset % 512;

    fat16_read_sector(fs, fat_sector, sector_buf);
    return *(uint16_t *)(sector_buf + fat_entry_offset);
}

static int fat16_read_cluster(fat16_fs_t *fs, uint16_t cluster, void *buf) {
    uint32_t lba = fs->data_start + (cluster - 2) * fs->sectors_per_cluster;
    return ata_read_sectors(fs->device,
                            fs->partition_lba + lba,
                            fs->sectors_per_cluster,
                            buf);
}

static void fat16_format_name(const fat16_dir_entry_t *entry, char *out) {
    int j = 0;
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++)
        out[j++] = tolower(entry->name[i]);

    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++)
            out[j++] = tolower(entry->ext[i]);
    }
    out[j] = '\0';
}

/* VFS operations for FAT16 files */
static int fat16_file_read(vfs_node_t *node, uint32_t offset,
                           uint32_t size, void *buf) {
    fat16_fs_t *fs = (fat16_fs_t *)node->private_data;
    /* Simplified: read entire file into temp buffer */
    uint32_t cluster = (uint32_t)(uintptr_t)node->inode;
    uint32_t cluster_size = fs->sectors_per_cluster * fs->bytes_per_sector;
    uint8_t *temp = (uint8_t *)kmalloc(node->length);

    uint32_t pos = 0;
    while (cluster >= 2 && cluster < 0xFFF8 && pos < node->length) {
        fat16_read_cluster(fs, cluster, temp + pos);
        pos += cluster_size;
        cluster = fat16_get_cluster(fs, cluster);
    }

    uint32_t to_read = MIN(size, node->length - offset);
    memcpy(buf, temp + offset, to_read);
    kfree(temp);
    return to_read;
}

static vfs_node_t *fat16_dir_finddir(vfs_node_t *node, const char *name) {
    fat16_fs_t *fs = (fat16_fs_t *)node->private_data;
    uint32_t cluster = (uint32_t)(uintptr_t)node->inode;

    uint8_t *dir_buf = (uint8_t *)kmalloc(fs->sectors_per_cluster * 512);
    uint32_t entries_per_cluster = (fs->sectors_per_cluster * 512) / 32;
    (void)entries_per_cluster;

    /* For root directory, read from root_dir_start */
    if (cluster == 0) {
        uint32_t root_sectors = (fs->root_entry_count * 32 + 511) / 512;
        uint8_t *root_buf = (uint8_t *)kmalloc(root_sectors * 512);
        ata_read_sectors(fs->device,
                         fs->partition_lba + fs->root_dir_start,
                         root_sectors, root_buf);

        for (uint32_t i = 0; i < fs->root_entry_count; i++) {
            fat16_dir_entry_t *entry = (fat16_dir_entry_t *)(root_buf + i * 32);
            if ((unsigned char)entry->name[0] == 0) break;
            if ((unsigned char)entry->name[0] == 0xE5) continue;
            if ((entry->attr & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;

            char formatted[13];
            fat16_format_name(entry, formatted);

            if (strcmp(formatted, name) == 0) {
                vfs_node_t *child = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
                strncpy(child->name, formatted, VFS_NAME_MAX);
                child->length = entry->file_size;
                child->inode = entry->first_cluster_lo;
                child->private_data = fs;
                child->parent = node;

                if (entry->attr & FAT_ATTR_DIRECTORY) {
                    child->flags = VFS_DIRECTORY;
                    child->finddir = fat16_dir_finddir;
                } else {
                    child->flags = VFS_FILE;
                    child->read = fat16_file_read;
                }

                kfree(root_buf);
                kfree(dir_buf);
                return child;
            }
        }
        kfree(root_buf);
    }

    kfree(dir_buf);
    return NULL;
}

vfs_node_t *fat16_mount(ata_device_t *dev, uint32_t partition_lba) {
    /* Read BPB (Boot Parameter Block) */
    fat16_bpb_t bpb;
    if (ata_read_sectors(dev, partition_lba, 1, &bpb) != 0) {
        kprintf("[FAT16] Failed to read boot sector\n");
        return NULL;
    }

    /* Validate */
    if (bpb.bytes_per_sector != 512) {
        kprintf("[FAT16] Unsupported sector size: %d\n", bpb.bytes_per_sector);
        return NULL;
    }

    fat16_fs_t *fs = (fat16_fs_t *)kzalloc(sizeof(fat16_fs_t));
    fs->device = dev;
    fs->partition_lba = partition_lba;
    fs->bytes_per_sector = bpb.bytes_per_sector;
    fs->sectors_per_cluster = bpb.sectors_per_cluster;
    fs->reserved_sectors = bpb.reserved_sectors;
    fs->num_fats = bpb.num_fats;
    fs->fat_size = bpb.fat_size_16;
    fs->root_entry_count = bpb.root_entry_count;

    fs->fat_start = fs->reserved_sectors;
    fs->root_dir_start = fs->fat_start + (fs->num_fats * fs->fat_size);

    uint32_t root_dir_sectors = (fs->root_entry_count * 32 + 511) / 512;
    fs->data_start = fs->root_dir_start + root_dir_sectors;

    uint32_t total_sectors = bpb.total_sectors_16 ?
                             bpb.total_sectors_16 : bpb.total_sectors_32;
    fs->total_clusters = (total_sectors - fs->data_start) / fs->sectors_per_cluster;

    kprintf("[FAT16] Mounted: %u clusters, %u bytes/cluster\n",
            fs->total_clusters,
            fs->sectors_per_cluster * fs->bytes_per_sector);

    /* Create VFS root node */
    vfs_node_t *root = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(root->name, "fat16");
    root->flags = VFS_DIRECTORY;
    root->inode = 0;    /* Root dir has no cluster */
    root->private_data = fs;
    root->finddir = fat16_dir_finddir;

    return root;
}

