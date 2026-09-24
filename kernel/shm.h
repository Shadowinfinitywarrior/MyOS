#ifndef SHM_H
#define SHM_H

#include "../include/types.h"

#define SHM_MAX_SEGMENTS 32
#define SHM_NAME_MAX     32

int      shm_create(const char *name, uint32_t size);
int      shm_open(const char *name);
void    *shm_attach(int shmid);
int      shm_detach(int shmid, void *addr);
int      shm_destroy(int shmid);
void     shm_init(void);

#endif
