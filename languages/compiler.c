#include "mylang.h"
#include "../lib/string.h"
#include "../lib/printf.h"

static void emit(mylang_program_t *p, uint8_t op){ p->code[p->code_len++] = op; }

int mylang_compile(const char *src, mylang_program_t *out){
    memset(out,0,sizeof(*out));
    if(!src) return -1;
    
    /* Minimal compiler: parses "push N; add; print" */
    const char *p = src;
    while(*p){
        while(*p==' '||*p=='\n') p++;
        if(strncmp(p,"push",4)==0){
            emit(out,OP_PUSH);
            p+=4;
            while(*p==' ') p++;
            char numbuf[32]={0};
            int i=0;
            while(*p>='0'&&*p<='9'&&i<31) numbuf[i++]=*p++;
            numbuf[i]=0;
            int val = atoi(numbuf);
            emit(out,(val>>24)&0xFF);
            emit(out,(val>>16)&0xFF);
            emit(out,(val>>8)&0xFF);
            emit(out,val&0xFF);
        } else if(strncmp(p,"add",3)==0){
            emit(out,OP_ADD); p+=3;
        } else if(strncmp(p,"print",5)==0){
            emit(out,OP_PRINT); p+=5;
        } else if(strncmp(p,"halt",4)==0){
            emit(out,OP_HALT); p+=4;
        } else {
            p++;
        }
    }
    if(out->code_len==0) emit(out,OP_HALT);
    return 0;
}
