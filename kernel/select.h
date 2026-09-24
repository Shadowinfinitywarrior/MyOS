#ifndef SELECT_H
#define SELECT_H
#include "../include/types.h"
#define FD_SETSIZE 64
typedef struct { uint64_t bits; } fd_set_t;
#define FD_ZERO(s) ((s)->bits=0)
#define FD_SET(f,s) ((s)->bits|=1ULL<<(f))
#define FD_CLR(f,s) ((s)->bits&=~(1ULL<<(f)))
#define FD_ISSET(f,s) (((s)->bits>>(f))&1)
#define POLLIN 0x001
#define POLLOUT 0x004
typedef struct { int fd; uint16_t events; uint16_t revents; } pollfd_t;
int sys_select(int nfds, fd_set_t *r, fd_set_t *w, fd_set_t *e, uint32_t to);
int sys_poll(pollfd_t *fds, uint32_t nfds, int32_t to);
#endif
