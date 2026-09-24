#ifndef RWLOCK_H
#define RWLOCK_H
#include "../include/types.h"
typedef struct { volatile int readers; volatile int writer; } rwlock_t;
static inline void rwlock_init(rwlock_t* r){ r->readers=0; r->writer=0; }
static inline void rwlock_rdlock(rwlock_t* r){ while(__atomic_load_n(&r->writer,__ATOMIC_ACQUIRE)); __atomic_add_fetch(&r->readers,1,__ATOMIC_ACQUIRE); }
static inline void rwlock_rdunlock(rwlock_t* r){ __atomic_sub_fetch(&r->readers,1,__ATOMIC_RELEASE); }
static inline void rwlock_wrlock(rwlock_t* r){ while(__atomic_test_and_set(&r->writer,__ATOMIC_ACQUIRE)); }
static inline void rwlock_wrunlock(rwlock_t* r){ __atomic_clear(&r->writer,__ATOMIC_RELEASE); }
#endif
