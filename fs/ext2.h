#ifndef EXT2_H
#define EXT2_H

#include "../include/types.h"

#define EXT2_BLOCK_SIZE 1024
#define EXT2_MAGIC      0xEF53

typedef struct ext2_super_block {
    uint32_t total_inodes;
    uint32_t total_blocks;
    uint32_t unused_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint32_t def_resuid;
    uint32_t def_resgid;
    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    char     vol_name[16];
    char     last_mounted[64];
} PACKED ext2_super_block_t;

typedef struct ext2_group_desc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t used_dirs;
    uint16_t pad;
    uint32_t reserved[3];
} PACKED ext2_group_desc_t;

typedef struct ext2_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t os1;
    uint32_t direct[12];
    uint32_t indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t fragment_addr;
    uint8_t  os2[12];
} PACKED ext2_inode_t;

typedef struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} PACKED ext2_dir_entry_t;

typedef struct ext2_fs {
    ext2_super_block_t *sb;
    ext2_group_desc_t  *gd;
    uint32_t            block_size;
} ext2_fs_t;

int ext2_init(void);
int ext2_mount(uint32_t start_sector);
int ext2_read_file(const char *path, uint8_t *buf, uint32_t *size);
int ext2_write_file(const char *path, const uint8_t *buf, uint32_t size);
int ext2_create_file(const char *path);
int ext2_delete_file(const char *path);
int ext2_list_dir(const char *path, char *buf, uint32_t buf_size);
int ext2_create_dir_entry(uint32_t dir_inode_num, uint32_t child_inode_num, const char *name);
int ext2_mkdir(const char *path);

#endif
