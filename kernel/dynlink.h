#ifndef DYNLINK_H
#define DYNLINK_H
#include "../include/types.h"
int elf_dynlink_load(const char* path);
void* dlsym(void* handle,const char* sym);
void* dlopen(const char* path);
int dlclose(void* handle);
#endif
