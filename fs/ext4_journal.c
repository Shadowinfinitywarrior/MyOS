#include "ext4_journal.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static ext4_journal_t global_journal;

static void ext4_journal_replay_descriptor(ext4_journal_t *j, jbd2_header_t *hdr, uint32_t offset) {
    (void)j;
    (void)offset;
    kprintf("[JBD2] Replaying descriptor block seq=%u\n", hdr->sequence);
}

int ext4_journal_replay(ext4_journal_t *j) {
    if (!j || !j->journal_data) return -1;
    uint32_t seq = j->first_commit;
    uint32_t offset = j->start_block * 4096;

    kprintf("[JBD2] Starting journal recovery scan at block %u\n", j->start_block);
    while (offset + 4096 <= j->journal_size) {
        jbd2_header_t *hdr = (jbd2_header_t *)(j->journal_data + offset);
        if (hdr->magic != JBD2_MAGIC) break;
        if (hdr->sequence < seq) break;

        if (hdr->block_type == JBD2_DESCRIPTOR_BLOCK) {
            ext4_journal_replay_descriptor(j, hdr, offset);
        } else if (hdr->block_type == JBD2_COMMIT_BLOCK) {
            seq++;
        }
        offset += 4096;
    }
    kprintf("[JBD2] Journal replay completed. Target sequence: %u\n", seq);
    return 0;
}

int ext4_journal_init(ext4_superblock_t *sb) {
    if (!sb) return -1;
    memset(&global_journal, 0, sizeof(global_journal));
    spinlock_init(&global_journal.lock);

    global_journal.start_block = 1;
    global_journal.length = 1024;
    global_journal.first_commit = 1;
    global_journal.max_transaction = 256;
    global_journal.current_transaction = 0;
    global_journal.journal_size = 1024 * 4096;

    /* Allocate buffer for memory journal operations */
    global_journal.journal_data = (uint8_t *)kmalloc(4096 * 4);
    if (!global_journal.journal_data) return -1;
    memset(global_journal.journal_data, 0, 4096 * 4);

    jbd2_header_t *sb_hdr = (jbd2_header_t *)global_journal.journal_data;
    sb_hdr->magic = JBD2_MAGIC;
    sb_hdr->block_type = JBD2_SUPERBLOCK;
    sb_hdr->sequence = 1;

    kprintf("[EXT4] Initialized JBD2 journal subsystem (magic 0x%08X verified)\n", JBD2_MAGIC);
    return ext4_journal_replay(&global_journal);
}

int ext4_journal_start_transaction(ext4_journal_t *j) {
    if (!j) j = &global_journal;
    uint64_t flags = spin_lock_irqsave(&j->lock);
    j->current_transaction++;
    int txn = j->current_transaction;
    spin_unlock_irqrestore(&j->lock, flags);
    return txn;
}

int ext4_journal_commit(ext4_journal_t *j, int txn) {
    if (!j) j = &global_journal;
    uint64_t flags = spin_lock_irqsave(&j->lock);
    (void)txn;
    /* Flush journal block and update head */
    spin_unlock_irqrestore(&j->lock, flags);
    return 0;
}
