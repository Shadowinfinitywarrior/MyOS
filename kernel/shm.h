#ifndef SHM_H
#define SHM_H

#include "../include/types.h"
#include "process.h"

#define SHM_MAX_SEGMENTS 64
#define SHM_NAME_MAX     64

/* Shared memory segment flags */
#define SHM_FLAG_READONLY  (1 << 0)
#define SHM_FLAG_FRAMEBUF  (1 << 1)  /* Framebuffer-backed segment */
#define SHM_FLAG_CURSOR    (1 << 2)  /* Hardware cursor buffer */
#define SHM_FLAG_ZERO_COPY (1 << 3)  /* Zero-copy blitting enabled */

typedef struct shm_segment {
    char name[SHM_NAME_MAX];
    uint32_t size;
    uint32_t phys_addr;
    uint32_t ref_count;
    uint32_t flags;
    pid_t    owner_pid;
    bool     in_use;
} shm_segment_t;

/* Framebuffer shared memory info */
typedef struct shm_fb_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;  /* ARGB8888 = 1 */
    uint64_t phys_addr;
    uint32_t size;
} shm_fb_info_t;

/* Access to segment array for mmap integration */
extern shm_segment_t *shm_get_segments(void);

int      shm_create(const char *name, uint32_t size);
int      shm_create_framebuffer(uint32_t width, uint32_t height, uint32_t bpp);
int      shm_open(const char *name);
int      shm_unlink(const char *name);
void    *shm_attach(int shmid);
int      shm_detach(int shmid, void *addr);
int      shm_destroy(int shmid);
void     shm_init(void);

/* Framebuffer operations */
int      shm_get_framebuffer(void);
void    *shm_map_framebuffer(process_t *proc, uint32_t addr);

/* Get shmid from file descriptor (used by mmap) */
int      get_shmid_from_fd(process_t *proc, int fd);

/* Statistics */
void     shm_dump_stats(void);

#endif