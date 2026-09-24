#include "exec.h"
#include "../lib/printf.h"
#include "../fs/vfs.h"
#include "heap.h"
#include "elf.h"
#include "paging.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int sys_exec(const char *path,const char **argv,const char **envp){
    kprintf("[EXEC] exec %s\n",path);
    (void)argv;(void)envp; // unused for now

    /* Resolve path in VFS */
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("[EXEC] path not found: %s\n", path);
        return -1;
    }

    /* Allocate buffer 
     * The file size is stored in node->length 
     */
    uint32_t size = node->length;
    if (size == 0) {
        kprintf("[EXEC] empty file: %s\n", path);
        return -1;
    }

    /* Allocate temporary buffer on kernel heap */
    uint8_t *data = (uint8_t *)kmalloc(size);
    if (!data) {
        kprintf("[EXEC] kmalloc failed for %u bytes\n", size);
        return -1;
    }

    int bytes = vfs_read(node, 0, size, data);
    if (bytes < 0 || (uint32_t)bytes != size) {
        kprintf("[EXEC] read failed for %s (%d bytes)\n", path, bytes);
        kfree(data);
        return -1;
    }

    uint32_t entry = elf_load(paging_get_directory(), data, size);
    kfree(data);
    if (!entry) {
        kprintf("[EXEC] ELF load failed for %s\n", path);
        return -1;
    }
    kprintf("[EXEC] ELF entry at 0x%08X\n", entry);
    return (int)entry;
}
