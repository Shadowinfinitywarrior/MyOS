#ifndef MMAP_H
#define MMAP_H

#include "../include/types.h"
#include "process.h"

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

#define MAP_FAILED    ((void *)-1)

typedef struct mmap_region {
    uint32_t             start;
    uint32_t             length;
    uint32_t             prot;
    uint32_t             flags;
    int                  fd;
    uint32_t             offset;
    struct mmap_region  *next;
} mmap_region_t;

void   mmap_init(void);
void  *sys_mmap(process_t *proc, uint32_t addr, uint32_t length,
                uint32_t prot, uint32_t flags, int fd, uint32_t offset);
int    sys_munmap(process_t *proc, uint32_t addr, uint32_t length);
int    sys_mprotect(process_t *proc, uint32_t addr, uint32_t length, uint32_t prot);

/* Get shmid from file descriptor (used by syscall_mmap) */
int get_shmid_from_fd(process_t *proc, int fd);

#endif
