#include "dynlink.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int elf_dynlink_load(const char* p){ kprintf("[dynlink] stub %s\n",p); return 0; }
void* dlsym(void*h,const char*s){ (void)h;(void)s; return NULL; }
void* dlopen(const char* p){ (void)p; return (void*)1; }
int dlclose(void*h){ (void)h; return 0; }
