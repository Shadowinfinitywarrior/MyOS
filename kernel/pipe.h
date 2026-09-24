#ifndef PIPE_H
#define PIPE_H
#include "../include/types.h"
#define PIPE_BUF 4096
#define MAX_PIPES 64
#define PIPE_FD_BASE 128
#define PIPE_READ 0
#define PIPE_WRITE 1
#define IS_PIPE(fd) ((fd)>=PIPE_FD_BASE && (fd)<PIPE_FD_BASE+MAX_PIPES*2)
typedef struct {
    uint8_t buf[PIPE_BUF];
    uint32_t r,w,cnt;
    uint32_t readers,writers;
    bool in_use;
} pipe_t;
void pipe_init(void);
int pipe_create(int fds[2]);
int pipe_read(int fd,void *buf,uint32_t cnt);
int pipe_write(int fd,const void *buf,uint32_t cnt);
void pipe_close(int fd);
int pipe_is_readable(int fd);
int pipe_is_writable(int fd);
#endif
