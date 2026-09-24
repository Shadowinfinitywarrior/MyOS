#ifndef EXT4_H
#define EXT4_H
#include "../include/types.h"
#include "vfs.h"
#define EXT4_MAGIC 0xEF53
#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL 0x0004
#define EXT4_EXT_MAGIC 0xF30A
typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_blocks_per_group;
    uint32_t s_inodes_per_group;
    uint16_t s_magic;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    uint32_t s_journal_inum;
} PACKED ext4_superblock_t;
typedef struct {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} PACKED ext4_extent_header_t;
typedef struct {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} PACKED ext4_extent_t;
typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_block[15];
    uint32_t i_size_hi;
} PACKED ext4_inode_t;
typedef struct {
    void* device;
    uint32_t partition_lba;
    ext4_superblock_t sb;
    uint32_t block_size;
    uint32_t inode_size;
    bool has_extents;
    bool is_64bit;
    bool has_journal;
    uint8_t* block_buf;
} ext4_fs_t;
vfs_node_t* ext4_mount(void* dev,uint32_t partition_lba);
#endif
