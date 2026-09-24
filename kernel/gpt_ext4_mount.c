#include "gpt.h"
#include "../fs/ext4.h"
#include "../drivers/ata.h"
#include "../lib/printf.h"
#include "../kernel/heap.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern ata_device_t* ata_devices[];

static void mount_ext4_for_partitions(void){
    int count = gpt_get_partition_count();
    if(count<=0) return;
    for(int i=0;i<count;i++){
        partition_info_t* p = gpt_get_partition(i);
        if(!p || !p->valid) continue;
        /* Linux filesystem GUID: AF3DC6... */
        if(p->type_guid[0]==0xAF && p->type_guid[1]==0x3D && p->type_guid[2]==0xC6){
            kprintf("[EXT4] Mounting partition %d at LBA %lu\n", i, p->start_lba);
            if(ata_devices[0]){
                vfs_node_t* root = ext4_mount(ata_devices[0], (uint32_t)p->start_lba);
                if(root){
                    kprintf("[EXT4] Mounted root at %s\n", root->name);
                } else {
                    kprintf("[EXT4] Mount failed for partition %d\n", i);
                }
            }
        }
    }
}
void gpt_mount_ext4(void){
    mount_ext4_for_partitions();
}
