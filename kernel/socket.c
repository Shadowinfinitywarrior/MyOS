#include "socket.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static socket_t sockets[MAX_SOCKETS];
void socket_init(void){ memset(sockets,0,sizeof(sockets)); kprintf("[SOCKET] init\n"); }
static socket_t* alloc_socket(void){ for(int i=0;i<MAX_SOCKETS;i++) if(!sockets[i].in_use){ memset(&sockets[i],0,sizeof(socket_t)); sockets[i].in_use=true; sockets[i].fd=i+2000; return &sockets[i]; } return NULL; }
static socket_t* get_socket(int fd){ int i=fd-2000; if(i>=0&&i<MAX_SOCKETS&&sockets[i].in_use) return &sockets[i]; return NULL; }
int sys_socket(int d,int t,int p){ (void)p; socket_t* s=alloc_socket(); if(!s) return -1; s->domain=d; s->type=t; return s->fd; }
int sys_bind(int fd,const void* a,uint32_t l){ (void)a;(void)l; socket_t* s=get_socket(fd); if(!s) return -1; return 0; }
int sys_listen(int fd,int b){ (void)b; socket_t* s=get_socket(fd); if(!s) return -1; s->state=2; return 0; }
int sys_accept(int fd,void* a,uint32_t* l){ (void)a;(void)l; socket_t* s=get_socket(fd); if(!s) return -1; socket_t* c=alloc_socket(); if(!c) return -1; c->state=3; return c->fd; }
int sys_connect(int fd,const void* a,uint32_t l){ (void)a;(void)l; socket_t* s=get_socket(fd); if(!s) return -1; s->state=3; return 0; }
int sys_socket_read(int fd,void* b,uint32_t c){ (void)b;(void)c; socket_t* s=get_socket(fd); if(!s) return -1; return 0; }
int sys_socket_write(int fd,const void* b,uint32_t c){ (void)b;(void)c; socket_t* s=get_socket(fd); if(!s) return -1; return 0; }
void sys_socket_close(int fd){ socket_t* s=get_socket(fd); if(s) s->in_use=false; }
