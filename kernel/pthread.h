#ifndef PTHREAD_H
#define PTHREAD_H
#include "../include/types.h"
typedef int pthread_t;
typedef void* (*pthread_start_t)(void*);
int pthread_create(pthread_t* t,pthread_start_t start,void* arg);
int pthread_join(pthread_t t,void** rv);
int pthread_yield(void);
#endif
