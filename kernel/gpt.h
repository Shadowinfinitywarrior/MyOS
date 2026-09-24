#ifndef GPT_H
#define GPT_H
#include "../include/types.h"
#define GPT_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_LBA 1
#define GPT_MAX_PARTITIONS 128
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partitions;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
    uint8_t reserved2[420];
} PACKED gpt_header_t;
typedef struct {
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];
} PACKED gpt_entry_t;
typedef struct {
    char name[36];
    uint8_t type_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t size_sectors;
    uint64_t attributes;
    int index;
    bool valid;
} partition_info_t;
int gpt_detect(int (*read_fn)(uint64_t lba, uint32_t count, void* buf));
int gpt_get_partition_count(void);
partition_info_t* gpt_get_partition(int index);
void gpt_list_partitions(void);
#endif
