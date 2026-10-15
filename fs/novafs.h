#ifndef NOVAFS_H
#define NOVAFS_H

#include "../include/types.h"
#include "vfs.h"

#define NOVAFS_MAGIC            0x4E4F5641  /* "NOVA" in ASCII */
#define NOVAFS_VERSION          1
#define NOVAFS_DEFAULT_BSIZE    1024
#define NOVAFS_MAX_NAME         60
#define NOVAFS_INODE_EXTENTS    4
#define NOVAFS_INLINE_MAX       64

/* Feature flags */
#define NOVAFS_FEAT_EXTENTS     0x0001
#define NOVAFS_FEAT_INLINE      0x0002
#define NOVAFS_FEAT_JOURNAL     0x0004
#define NOVAFS_FEAT_CRC32       0x0008

/* Inode flags */
#define NOVAFS_IF_INLINE        0x0001
#define NOVAFS_IF_COMPRESSED    0x0002
#define NOVAFS_IF_IMMUTABLE     0x0004

/* Journal transaction types */
#define NOVAFS_TX_CREATE        1
#define NOVAFS_TX_WRITE         2
#define NOVAFS_TX_UNLINK        3
#define NOVAFS_TX_MKDIR         4
#define NOVAFS_TX_COMMIT        0xFF

#pragma pack(push, 1)

typedef struct {
    uint32_t block_offset;      /* Logical block offset in file */
    uint32_t start_block;       /* Physical block number on volume */
    uint16_t length;            /* Number of contiguous blocks */
    uint16_t flags;             /* Extent flags */
} novafs_extent_t;

typedef struct {
    uint16_t i_mode;            /* File mode (VFS_FILE, VFS_DIRECTORY) */
    uint16_t i_links;           /* Link count */
    uint32_t i_uid;             /* Owner UID */
    uint32_t i_gid;             /* Owner GID */
    uint64_t i_size;            /* File size in bytes */
    uint64_t i_atime;           /* Access time */
    uint64_t i_mtime;           /* Modification time */
    uint64_t i_ctime;           /* Change time */
    uint32_t i_flags;           /* NOVAFS_IF_* */
    union {
        novafs_extent_t extents[NOVAFS_INODE_EXTENTS];
        uint8_t         inline_data[NOVAFS_INLINE_MAX];
    } i_data;
    uint32_t i_checksum;        /* CRC32 of inode metadata */
} novafs_inode_t;

typedef struct {
    uint32_t s_magic;           /* NOVAFS_MAGIC */
    uint32_t s_version;         /* NOVAFS_VERSION */
    uint32_t s_block_size;      /* Block size (e.g. 1024) */
    uint32_t s_blocks_count;    /* Total blocks */
    uint32_t s_free_blocks;     /* Free blocks available */
    uint32_t s_inodes_count;    /* Total inodes allocated */
    uint32_t s_free_inodes;     /* Free inodes available */
    uint32_t s_root_inode;      /* Root directory inode (usually 1) */
    uint32_t s_journal_block;   /* Block offset for journal log */
    uint32_t s_journal_size;    /* Total blocks reserved for journal */
    uint32_t s_journal_head;    /* Active journal write pointer */
    uint64_t s_volume_uuid[2];  /* 128-bit filesystem UUID */
    char     s_volume_name[32]; /* Volume label */
    uint32_t s_features;        /* NOVAFS_FEAT_* */
    uint32_t s_mount_count;     /* Lifetime mount count */
    uint32_t s_checksum;        /* CRC32 of superblock */
} novafs_super_t;

typedef struct {
    uint32_t d_inode;           /* Inode number (0 = deleted/empty) */
    uint16_t d_rec_len;         /* Total record length */
    uint8_t  d_name_len;        /* Length of name string */
    uint8_t  d_file_type;       /* 1 = File, 2 = Directory, 3 = Symlink */
    char     d_name[NOVAFS_MAX_NAME]; /* Name (null terminated) */
} novafs_dirent_t;

typedef struct {
    uint32_t tx_id;             /* Monotonic transaction sequence ID */
    uint16_t tx_type;           /* NOVAFS_TX_* */
    uint16_t tx_flags;
    uint32_t tx_inode;          /* Affected inode */
    uint32_t tx_block;          /* Affected block */
    uint32_t tx_checksum;       /* Checksum of transaction entry */
} novafs_journal_entry_t;

#pragma pack(pop)

/* Public API */
int         novafs_init(void);
int         novafs_format(uint32_t blocks, const char *label);
int         novafs_mount_at(const char *mountpoint);
vfs_node_t *novafs_get_root(void);
int         novafs_fsck(void);
void        novafs_print_stats(void);

#endif /* NOVAFS_H */
