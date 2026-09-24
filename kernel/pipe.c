#include "pipe.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static pipe_t pipes[MAX_PIPES];
#define PID(fd) (((fd)-PIPE_FD_BASE)/2)
#define END(fd) (((fd)-PIPE_FD_BASE)%2)
void pipe_init(void){ memset(pipes,0,sizeof(pipes)); kprintf("[PIPE] init\n"); }
int pipe_create(int fds[2]){
    for(int i=0;i<MAX_PIPES;i++) if(!pipes[i].in_use){
        pipes[i].in_use=1; pipes[i].readers=1; pipes[i].writers=1;
        pipes[i].r=pipes[i].w=pipes[i].cnt=0;
        fds[0]=PIPE_FD_BASE+i*2+PIPE_READ;
        fds[1]=PIPE_FD_BASE+i*2+PIPE_WRITE;
        return 0;
    }
    return -1;
}
int pipe_read(int fd,void *b,uint32_t n){
    if(!IS_PIPE(fd)||END(fd)!=PIPE_READ) return -1;
    pipe_t *p=&pipes[PID(fd)];
    if(!p->in_use) return -1;
    uint8_t *dst=b; uint32_t r=0;
    while(p->cnt==0){ if(p->writers==0) return 0; }
    while(r<n && p->cnt){
        dst[r++]=p->buf[p->r];
        p->r=(p->r+1)%PIPE_BUF; p->cnt--;
    }
    return r;
}
int pipe_write(int fd,const void *b,uint32_t n){
    if(!IS_PIPE(fd)||END(fd)!=PIPE_WRITE) return -1;
    pipe_t *p=&pipes[PID(fd)];
    if(!p->in_use) return -1;
    if(p->readers==0){ kprintf("[PIPE] SIGPIPE\n"); return -1; }
    const uint8_t *src=b; uint32_t w=0;
    while(w<n){
        while(p->cnt>=PIPE_BUF){ if(p->readers==0) return -1; }
        p->buf[p->w]=src[w++];
        p->w=(p->w+1)%PIPE_BUF; p->cnt++;
    }
    return w;
}
void pipe_close(int fd){
    if(!IS_PIPE(fd)) return;
    pipe_t *p=&pipes[PID(fd)];
    if(END(fd)==PIPE_READ) p->readers--; else p->writers--;
    if(p->readers==0 && p->writers==0) p->in_use=0;
}
int pipe_is_readable(int fd){
    if(!IS_PIPE(fd)||END(fd)!=PIPE_READ) return 0;
    pipe_t *p=&pipes[PID(fd)];
    return p->in_use && (p->cnt>0 || p->writers==0);
}
int pipe_is_writable(int fd){
    if(!IS_PIPE(fd)||END(fd)!=PIPE_WRITE) return 0;
    pipe_t *p=&pipes[PID(fd)];
    return p->in_use && p->cnt<PIPE_BUF;
}
