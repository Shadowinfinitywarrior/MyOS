#include "ext2.h"
#include "vfs.h"
#include "../lib/string.h"
#include "../drivers/virtio_blk.h"

#define EXT2_MAX_BLOCK_SIZE 8192
#define EXT2_MAX_BLOCK_SECTORS (EXT2_MAX_BLOCK_SIZE / 512)

static ext2_fs_t ext2fs;
static uint8_t sector_buf[EXT2_MAX_BLOCK_SIZE];
static uint8_t indirect_buf[EXT2_MAX_BLOCK_SIZE];
static ext2_super_block_t superblock;
static ext2_group_desc_t group_desc[128];
static uint32_t ext2_start_sector;

int ext2_init(void) {
    memset(&ext2fs, 0, sizeof(ext2_fs_t));
    ext2fs.block_size = EXT2_BLOCK_SIZE;
    return 0;
}

int ext2_mount(uint32_t start_sector) {
    ext2_start_sector = start_sector;
    virtio_blk_read(start_sector + 2, 2, sector_buf);
    memcpy(&superblock, sector_buf + 1024, sizeof(ext2_super_block_t));
    if (superblock.magic != EXT2_MAGIC) {
        return -1;
    }
    ext2fs.sb = &superblock;
    ext2fs.block_size = EXT2_BLOCK_SIZE << superblock.log_block_size;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t gd_sector = start_sector + 2 * sectors_per_block;
    virtio_blk_read(gd_sector, sectors_per_block, sector_buf);
    memcpy(group_desc, sector_buf, sizeof(group_desc));
    ext2fs.gd = group_desc;
    return 0;
}

static int ext2_get_inode(uint32_t inode_num, ext2_inode_t *inode) {
    if (!ext2fs.sb || !inode) return -1;
    uint32_t group = (inode_num - 1) / ext2fs.sb->inodes_per_group;
    uint32_t index = (inode_num - 1) % ext2fs.sb->inodes_per_group;
    uint32_t inode_table_block = ext2fs.gd[group].inode_table;
    uint32_t block_size = ext2fs.block_size;
    uint32_t offset = index * ext2fs.sb->inode_size;
    uint32_t block_offset = offset / block_size;
    uint32_t intra_offset = offset % block_size;
    uint32_t sectors_per_block = block_size / 512;
    uint32_t start_sector = ext2_start_sector + (inode_table_block + block_offset) * sectors_per_block + intra_offset / 512;
    uint32_t sectors_needed = (intra_offset % 512 + ext2fs.sb->inode_size + 511) / 512;
    virtio_blk_read(start_sector, sectors_needed, sector_buf);
    memcpy(inode, sector_buf + intra_offset % 512, ext2fs.sb->inode_size);
    return 0;
}

static int ext2_write_inode(uint32_t inode_num, ext2_inode_t *inode) {
    if (!ext2fs.sb || !inode) return -1;
    uint32_t group = (inode_num - 1) / ext2fs.sb->inodes_per_group;
    uint32_t index = (inode_num - 1) % ext2fs.sb->inodes_per_group;
    uint32_t inode_table_block = ext2fs.gd[group].inode_table;
    uint32_t block_size = ext2fs.block_size;
    uint32_t offset = index * ext2fs.sb->inode_size;
    uint32_t block_offset = offset / block_size;
    uint32_t intra_offset = offset % block_size;
    uint32_t sectors_per_block = block_size / 512;
    uint32_t start_sector = ext2_start_sector + (inode_table_block + block_offset) * sectors_per_block + intra_offset / 512;
    uint32_t sectors_needed = (intra_offset % 512 + ext2fs.sb->inode_size + 511) / 512;
    virtio_blk_read(start_sector, sectors_needed, sector_buf);
    memcpy(sector_buf + intra_offset % 512, inode, ext2fs.sb->inode_size);
    virtio_blk_write(start_sector, sectors_needed, sector_buf);
    return 0;
}

static int ext2_find_dir_entry(uint32_t dir_inode_num, const char *name, uint32_t *child_inode_num) {
    ext2_inode_t dir;
    if (ext2_get_inode(dir_inode_num, &dir) != 0) return -1;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t name_len = 0;
    while (name[name_len]) name_len++;
    for (int i = 0; i < 12; i++) {
        uint32_t blk = dir.direct[i];
        if (!blk) continue;
        uint32_t start_sector = ext2_start_sector + blk * sectors_per_block;
        virtio_blk_read(start_sector, sectors_per_block, sector_buf);
        uint32_t off = 0;
        while (off < ext2fs.block_size) {
            ext2_dir_entry_t *de = (ext2_dir_entry_t *)(sector_buf + off);
            if (de->inode == 0) break;
            if (de->name_len == name_len) {
                uint32_t j;
                for (j = 0; j < name_len; j++) {
                    if (de->name[j] != name[j]) break;
                }
                if (j == name_len) {
                    *child_inode_num = de->inode;
                    return 0;
                }
            }
            off += de->rec_len;
        }
    }
    return -1;
}

static int ext2_lookup_path(const char *path, uint32_t *inode_num, ext2_inode_t *inode) {
    if (!path || !inode_num) return -1;
    char buf[512];
    uint32_t i = 0;
    while (path[i] && i < 511) {
        buf[i] = path[i];
        i++;
    }
    buf[i] = '\0';
    char *p = buf;
    while (*p == '/') p++;
    uint32_t cur_inode_num = 2;
    ext2_inode_t cur_inode;
    if (ext2_get_inode(cur_inode_num, &cur_inode) != 0) return -1;
    if (*p == '\0') {
        *inode_num = cur_inode_num;
        if (inode) memcpy(inode, &cur_inode, sizeof(ext2_inode_t));
        return 0;
    }
    char *tok = p;
    char *sep;
    while (1) {
        sep = tok;
        while (*sep && *sep != '/') sep++;
        char save = *sep;
        *sep = '\0';
        uint32_t child;
        if (ext2_find_dir_entry(cur_inode_num, tok, &child) != 0) return -1;
        cur_inode_num = child;
        if (ext2_get_inode(cur_inode_num, &cur_inode) != 0) return -1;
        if (save == '\0') break;
        tok = sep + 1;
        while (*tok == '/') tok++;
        if (*tok == '\0') break;
    }
    *inode_num = cur_inode_num;
    if (inode) memcpy(inode, &cur_inode, sizeof(ext2_inode_t));
    return 0;
}

int ext2_read_file(const char *path, uint8_t *buf, uint32_t *size) {
    if (!path || !buf || !size) return -1;
    uint32_t inode_num;
    ext2_inode_t inode;
    if (ext2_lookup_path(path, &inode_num, &inode) != 0) return -1;
    if ((inode.mode & 0xF000) == 0x4000) return -1;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t off = 0;
    uint32_t remaining = inode.size;
    uint32_t to_read;
    uint32_t sectors_needed;
    for (int i = 0; i < 12 && remaining > 0; i++) {
        uint32_t blk = inode.direct[i];
        if (!blk) break;
        to_read = remaining > ext2fs.block_size ? ext2fs.block_size : remaining;
        sectors_needed = (to_read + 511) / 512;
        virtio_blk_read(ext2_start_sector + blk * sectors_per_block, sectors_needed, sector_buf);
        memcpy(buf + off, sector_buf, to_read);
        off += to_read;
        remaining -= to_read;
    }
    if (remaining > 0 && inode.indirect) {
        uint32_t indirect_blocks = ext2fs.block_size / sizeof(uint32_t);
        uint32_t *indirect;
        virtio_blk_read(ext2_start_sector + inode.indirect * sectors_per_block, sectors_per_block, indirect_buf);
        indirect = (uint32_t *)indirect_buf;
        for (uint32_t i = 0; i < indirect_blocks && remaining > 0; i++) {
            uint32_t blk = indirect[i];
            if (!blk) break;
            to_read = remaining > ext2fs.block_size ? ext2fs.block_size : remaining;
            sectors_needed = (to_read + 511) / 512;
            virtio_blk_read(ext2_start_sector + blk * sectors_per_block, sectors_needed, sector_buf);
            memcpy(buf + off, sector_buf, to_read);
            off += to_read;
            remaining -= to_read;
        }
    }
    if (remaining > 0 && inode.double_indirect) {
        uint32_t entries_per_block = ext2fs.block_size / sizeof(uint32_t);
        uint32_t *indirect1;
        uint32_t *indirect2;
        virtio_blk_read(ext2_start_sector + inode.double_indirect * sectors_per_block, sectors_per_block, indirect_buf);
        indirect1 = (uint32_t *)indirect_buf;
        for (uint32_t i = 0; i < entries_per_block && remaining > 0; i++) {
            uint32_t indirect_block = indirect1[i];
            if (!indirect_block) break;
            virtio_blk_read(ext2_start_sector + indirect_block * sectors_per_block, sectors_per_block, indirect_buf);
            indirect2 = (uint32_t *)indirect_buf;
            for (uint32_t j = 0; j < entries_per_block && remaining > 0; j++) {
                uint32_t blk = indirect2[j];
                if (!blk) break;
                to_read = remaining > ext2fs.block_size ? ext2fs.block_size : remaining;
                sectors_needed = (to_read + 511) / 512;
                virtio_blk_read(ext2_start_sector + blk * sectors_per_block, sectors_needed, sector_buf);
                memcpy(buf + off, sector_buf, to_read);
                off += to_read;
                remaining -= to_read;
            }
        }
    }
    if (remaining > 0 && inode.triple_indirect) {
        uint32_t entries_per_block = ext2fs.block_size / sizeof(uint32_t);
        uint32_t *indirect1;
        uint32_t *indirect2;
        uint32_t *indirect3;
        virtio_blk_read(ext2_start_sector + inode.triple_indirect * sectors_per_block, sectors_per_block, indirect_buf);
        indirect1 = (uint32_t *)indirect_buf;
        for (uint32_t i = 0; i < entries_per_block && remaining > 0; i++) {
            uint32_t indirect_block = indirect1[i];
            if (!indirect_block) break;
            virtio_blk_read(ext2_start_sector + indirect_block * sectors_per_block, sectors_per_block, indirect_buf);
            indirect2 = (uint32_t *)indirect_buf;
            for (uint32_t j = 0; j < entries_per_block && remaining > 0; j++) {
                uint32_t indirect_block2 = indirect2[j];
                if (!indirect_block2) break;
                virtio_blk_read(ext2_start_sector + indirect_block2 * sectors_per_block, sectors_per_block, indirect_buf);
                indirect3 = (uint32_t *)indirect_buf;
                for (uint32_t k = 0; k < entries_per_block && remaining > 0; k++) {
                    uint32_t blk = indirect3[k];
                    if (!blk) break;
                    to_read = remaining > ext2fs.block_size ? ext2fs.block_size : remaining;
                    sectors_needed = (to_read + 511) / 512;
                    virtio_blk_read(ext2_start_sector + blk * sectors_per_block, sectors_needed, sector_buf);
                    memcpy(buf + off, sector_buf, to_read);
                    off += to_read;
                    remaining -= to_read;
                }
            }
        }
    }
    *size = inode.size - remaining;
    return 0;
}

int ext2_write_file(const char *path, const uint8_t *buf, uint32_t size) {
    if (!path || !buf) return -1;
    uint32_t inode_num;
    ext2_inode_t inode;
    if (ext2_lookup_path(path, &inode_num, &inode) != 0) return -1;
    if ((inode.mode & 0xF000) == 0x4000) return -1;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t off = 0;
    uint32_t remaining = size;
    uint32_t to_write;
    for (int i = 0; i < 12 && remaining > 0; i++) {
        uint32_t blk = inode.direct[i];
        if (!blk) break;
        to_write = remaining > ext2fs.block_size ? ext2fs.block_size : remaining;
        uint32_t start_sector = ext2_start_sector + blk * sectors_per_block;
        virtio_blk_read(start_sector, sectors_per_block, sector_buf);
        memcpy(sector_buf, buf + off, to_write);
        virtio_blk_write(start_sector, sectors_per_block, sector_buf);
        off += to_write;
        remaining -= to_write;
    }
    inode.size = size;
    ext2_write_inode(inode_num, &inode);
    return 0;
}

static int ext2_find_free_inode(uint32_t *inode_num) {
    if (!ext2fs.sb || !inode_num) return -1;
    uint32_t inodes_per_group = ext2fs.sb->inodes_per_group;
    uint32_t block_size = ext2fs.block_size;
    uint32_t sectors_per_block = block_size / 512;
    for (uint32_t g = 0; g < 128; g++) {
        if (ext2fs.gd[g].inode_bitmap == 0) continue;
        uint32_t start_sector = ext2_start_sector + ext2fs.gd[g].inode_bitmap * sectors_per_block;
        virtio_blk_read(start_sector, sectors_per_block, sector_buf);
        uint32_t bitmap_bytes = (inodes_per_group + 7) / 8;
        for (uint32_t i = 0; i < bitmap_bytes; i++) {
            uint8_t byte = sector_buf[i];
            if (byte == 0xFF) continue;
            for (uint32_t b = 0; b < 8; b++) {
                if (!(byte & (1 << b))) {
                    uint32_t idx = g * inodes_per_group + i * 8 + b;
                    if (idx == 0) continue;
                    *inode_num = idx + 1;
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int ext2_find_free_block(uint32_t *block_num) {
    if (!ext2fs.sb || !block_num) return -1;
    uint32_t blocks_per_group = ext2fs.sb->blocks_per_group;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    for (uint32_t g = 0; g < 128; g++) {
        if (ext2fs.gd[g].block_bitmap == 0) continue;
        uint32_t start_sector = ext2_start_sector + ext2fs.gd[g].block_bitmap * sectors_per_block;
        virtio_blk_read(start_sector, sectors_per_block, sector_buf);
        uint32_t bitmap_bytes = (blocks_per_group + 7) / 8;
        for (uint32_t i = 0; i < bitmap_bytes; i++) {
            uint8_t byte = sector_buf[i];
            if (byte == 0xFF) continue;
            for (uint32_t b = 0; b < 8; b++) {
                if (!(byte & (1 << b))) {
                    uint32_t idx = g * blocks_per_group + i * 8 + b;
                    if (idx == 0) continue;
                    *block_num = idx + 1;
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int ext2_alloc_block(uint32_t *block_num) {
    uint32_t block;
    if (ext2_find_free_block(&block) != 0) return -1;
    uint32_t blocks_per_group = ext2fs.sb->blocks_per_group;
    uint32_t g = (block - 1) / blocks_per_group;
    uint32_t idx_in_group = (block - 1) % blocks_per_group;
    uint32_t i = idx_in_group / 8;
    uint32_t b = idx_in_group % 8;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t start_sector = ext2_start_sector + ext2fs.gd[g].block_bitmap * sectors_per_block;
    virtio_blk_read(start_sector, sectors_per_block, sector_buf);
    sector_buf[i] |= (1 << b);
    virtio_blk_write(start_sector, sectors_per_block, sector_buf);
    if (ext2fs.sb->free_blocks > 0) ext2fs.sb->free_blocks--;
    if (ext2fs.gd[g].free_blocks > 0) ext2fs.gd[g].free_blocks--;
    *block_num = block;
    return 0;
}

static int ext2_put_inode(uint32_t inode_num, ext2_inode_t *inode) {
    return ext2_write_inode(inode_num, inode);
}

int ext2_update_superblock(void) {
    if (!ext2fs.sb) return -1;
    virtio_blk_read(ext2_start_sector + 2, 2, sector_buf);
    memcpy(sector_buf + 1024, ext2fs.sb, sizeof(ext2_super_block_t));
    virtio_blk_write(ext2_start_sector + 2, 2, sector_buf);
    return 0;
}

int ext2_create_file(const char *path) {
    if (!path || !ext2fs.sb) return -1;
    uint32_t inode_num;
    if (ext2_find_free_inode(&inode_num) != 0) return -1;
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    inode.mode = 0x8000;
    inode.uid = 0;
    inode.gid = 0;
    inode.links_count = 1;
    inode.size = 0;
    inode.atime = 0;
    inode.ctime = 0;
    inode.mtime = 0;
    ext2_put_inode(inode_num, &inode);
    if (ext2fs.sb->free_inodes > 0) ext2fs.sb->free_inodes--;
    uint32_t group = (inode_num - 1) / ext2fs.sb->inodes_per_group;
    if (group < 128 && ext2fs.gd[group].free_inodes > 0) ext2fs.gd[group].free_inodes--;
    ext2_update_superblock();
    return 0;
}

int ext2_create_dir_entry(uint32_t dir_inode_num, uint32_t child_inode_num, const char *name) {
    ext2_inode_t inode;
    if (ext2_get_inode(dir_inode_num, &inode) != 0) return -1;
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t block = inode.direct[0];
    if (!block) return -1;
    uint32_t start_sector = ext2_start_sector + block * sectors_per_block;
    virtio_blk_read(start_sector, sectors_per_block, sector_buf);
    uint32_t name_len = 0;
    while (name[name_len]) name_len++;
    uint32_t i = 0;
    for (i = 0; i < ext2fs.block_size; ) {
        ext2_dir_entry_t *de = (ext2_dir_entry_t *)(sector_buf + i);
        if (de->inode == 0) {
            uint32_t rec_len = 8 + ((name_len + 3) & ~3);
            if (i + rec_len > ext2fs.block_size) return -1;
            de->inode = child_inode_num;
            de->rec_len = rec_len;
            de->name_len = name_len;
            de->file_type = 1;
            uint32_t j;
            for (j = 0; j < name_len; j++) de->name[j] = name[j];
            virtio_blk_write(start_sector, sectors_per_block, sector_buf);
            return 0;
        }
        i += de->rec_len;
    }
    return -1;
}

int ext2_delete_file(const char *path) {
    if (!path) return -1;
    return 0;
}

int ext2_list_dir(const char *path, char *buf, uint32_t buf_size) {
    if (!path || !buf || buf_size == 0) return -1;
    return -1;
}

int ext2_mkdir(const char *path) {
    if (!path || !ext2fs.sb) return -1;
    const char *p = path;
    while (*p == '/') p++;
    if (*p == '\0') return -1;
    if (strchr(p, '/')) return -1;
    char name[256];
    uint32_t n = 0;
    while (*p && n < 255) {
        name[n++] = *p++;
    }
    name[n] = '\0';
    uint32_t inode_num;
    if (ext2_find_free_inode(&inode_num) != 0) return -1;
    uint32_t block_num;
    if (ext2_alloc_block(&block_num) != 0) return -1;
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    inode.mode = 0x41ED;
    inode.uid = 0;
    inode.gid = 0;
    inode.links_count = 2;
    inode.size = ext2fs.block_size;
    inode.direct[0] = block_num;
    inode.blocks = 2;
    ext2_put_inode(inode_num, &inode);
    uint32_t sectors_per_block = ext2fs.block_size / 512;
    uint32_t start_sector = ext2_start_sector + block_num * sectors_per_block;
    memset(sector_buf, 0, ext2fs.block_size);
    ext2_dir_entry_t *de = (ext2_dir_entry_t *)sector_buf;
    de->inode = inode_num;
    de->rec_len = 12;
    de->name_len = 1;
    de->file_type = 2;
    de->name[0] = '.';
    de = (ext2_dir_entry_t *)(sector_buf + 12);
    de->inode = 2;
    de->rec_len = ext2fs.block_size - 12;
    de->name_len = 2;
    de->file_type = 2;
    de->name[0] = '.';
    de->name[1] = '.';
    virtio_blk_write(start_sector, sectors_per_block, sector_buf);
    if (ext2_create_dir_entry(2, inode_num, name) != 0) return -1;
    if (ext2fs.sb->free_inodes > 0) ext2fs.sb->free_inodes--;
    uint32_t group = (inode_num - 1) / ext2fs.sb->inodes_per_group;
    if (group < 128 && ext2fs.gd[group].free_inodes > 0) ext2fs.gd[group].free_inodes--;
    ext2_update_superblock();
    return 0;
}
