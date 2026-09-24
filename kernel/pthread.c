#include "pthread.h"
#include "mutex.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int pthread_create(pthread_t* t,pthread_start_t start,void* arg){
    (void)t;(void)start;(void)arg;
    kprintf("[pthreads] create stub\n");
    return 0;
}
int pthread_join(pthread_t t,void** rv){ (void)t;(void)rv; return 0; }
int pthread_yield(void){ return 0; }
