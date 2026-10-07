/*
 * MicroPython thread port for MyOS
 * Stub implementation (no threading in bare-metal)
 */

#ifndef MPTHREADPORT_H
#define MPTHREADPORT_H

#include "types.h"

typedef int mp_thread_id_t;
typedef void *mp_thread_t;
typedef void (*mp_thread_entry_t)(void *);

// Thread creation - not supported, returns error
static inline int mp_thread_create(mp_thread_t *thread, mp_thread_entry_t entry, void *arg, size_t stack_size) {
    (void)thread; (void)entry; (void)arg; (void)stack_size;
    return -1; // Not supported
}

// Thread exit
static inline void mp_thread_exit(void) {
    // Not supported
}

// Thread join - not supported
static inline void mp_thread_join(mp_thread_t thread) {
    (void)thread;
}

// Get current thread ID
static inline mp_thread_id_t mp_thread_get_id(void) {
    return 0;
}

// Mutex stubs
typedef struct _mp_thread_mutex_t {
    int dummy;
} mp_thread_mutex_t;

static inline void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    (void)mutex;
}

static inline int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    (void)mutex; (void)wait;
    return 0;
}

static inline void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    (void)mutex;
}

static inline void mp_thread_mutex_deinit(mp_thread_mutex_t *mutex) {
    (void)mutex;
}

// Semaphore stubs (used for GIL)
typedef struct _mp_thread_sema_t {
    int dummy;
} mp_thread_sema_t;

static inline int mp_thread_sema_init(mp_thread_sema_t *sema, int value) {
    (void)sema; (void)value;
    return 0;
}

static inline void mp_thread_sema_deinit(mp_thread_sema_t *sema) {
    (void)sema;
}

static inline int mp_thread_sema_wait(mp_thread_sema_t *sema, uint32_t timeout_ms) {
    (void)sema; (void)timeout_ms;
    return 0;
}

static inline void mp_thread_sema_signal(mp_thread_sema_t *sema) {
    (void)sema;
}

// TLS (Thread Local Storage) - not supported
static inline void *mp_thread_get_tls(void) {
    return NULL;
}

static inline void mp_thread_set_tls(void *ptr) {
    (void)ptr;
}

#endif // MPTHREADPORT_H