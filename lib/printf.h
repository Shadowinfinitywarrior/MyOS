#ifndef PRINTF_H
#define PRINTF_H

#include "../include/types.h"

void kprintf(const char *fmt, ...);
int kputchar(int c);
int kputs(const char *s);
int ksprintf(char *buf, const char *fmt, ...);

static inline void kprint_hex(uint32_t v){
    kprintf("0x");
    for(int i=28;i>=0;i-=4){
        uint32_t nib = (v>>i)&0xF;
        kputchar(nib<10 ? '0'+nib : 'A'+(nib-10));
    }
}
#endif
