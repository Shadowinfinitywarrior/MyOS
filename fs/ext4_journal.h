#ifndef EXT4_JOURNAL_H
#define EXT4_JOURNAL_H

#include "../include/types.h"
#include "../include/spinlock.h"
#include "ext4.h"

#define JBD2_MAGIC 0xC03B3998

#define JBD2_DESCRIPTOR_BLOCK 1
#define JBD2_COMMIT_BLOCK     2
#define JBD2_REVOKE_BLOCK     3
#define JBD2_SUPERBLOCK       4

typedef struct {
    uint32_t magic;
    uint32_t block_type;   /* 1=descriptor, 2=commit, 3=revoke, 4=superblock */
    uint32_t sequence;
} PACKED jbd2_header_t;

typedef struct {
    jbd2_header_t header;
    uint32_t block_tag[0];
} PACKED jbd2_descriptor_t;

typedef struct {
    uint32_t start_block;
    uint32_t length;
    uint32_t first_commit;
    uint32_t max_transaction;
    uint32_t current_transaction;
    spinlock_t lock;
    uint8_t *journal_data;
    uint32_t journal_size;
} ext4_journal_t;

int ext4_journal_init(ext4_superblock_t *sb);
int ext4_journal_replay(ext4_journal_t *j);
int ext4_journal_start_transaction(ext4_journal_t *j);
int ext4_journal_commit(ext4_journal_t *j, int txn);

#endif
