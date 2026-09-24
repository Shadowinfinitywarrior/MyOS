#ifndef MUTEX_H
#define MUTEX_H
#include "../include/types.h"
typedef struct { volatile int locked; } mutex_t;
static inline void mutex_init(mutex_t* m){ m->locked=0; }
static inline void mutex_lock(mutex_t* m){ while(__atomic_test_and_set(&m->locked,__ATOMIC_ACQUIRE)); }
static inline void mutex_unlock(mutex_t* m){ __atomic_clear(&m->locked,__ATOMIC_RELEASE); }
#endif
