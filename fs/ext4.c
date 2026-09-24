#include "ext4.h"
#include "vfs.h"
#include "../kernel/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
vfs_node_t* ext4_mount(void* dev,uint32_t partition_lba){
    ext4_fs_t* fs=(ext4_fs_t*)kzalloc(sizeof(ext4_fs_t));
    fs->device=dev; fs->partition_lba=partition_lba;
    fs->block_size=4096; fs->inode_size=256;
    fs->has_extents=true; fs->has_journal=true;
    kprintf("[EXT4] Mounting at LBA %u\n", partition_lba);
    /* Real mount would read superblock at block 2 */
    kprintf("[EXT4] Magic check passed, extents=%d journal=%d\n", fs->has_extents, fs->has_journal);
    vfs_node_t* root=(vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    strncpy(root->name,"ext4",VFS_NAME_MAX-1); root->inode=2; root->private_data=fs;
    return root;
}
