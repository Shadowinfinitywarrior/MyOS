#include "select.h"
#include "pipe.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static int ready_read(int fd){ if(!IS_PIPE(fd)) return 1; return pipe_is_readable(fd); }
static int ready_write(int fd){ if(!IS_PIPE(fd)) return 1; return pipe_is_writable(fd); }
int sys_select(int nfds, fd_set_t *r, fd_set_t *w, fd_set_t *e, uint32_t to){
    (void)e;
    (void)to;
    fd_set_t ro={0}, wo={0};
    int cnt=0;
    for(int i=0;i<nfds && i<FD_SETSIZE;i++){
        if(r && FD_ISSET(i,r) && ready_read(i)){ FD_SET(i,&ro); cnt++; }
        if(w && FD_ISSET(i,w) && ready_write(i)){ FD_SET(i,&wo); cnt++; }
    }
    if(r) *r=ro;
    if(w) *w=wo;
    return cnt;
}
int sys_poll(pollfd_t *fds, uint32_t n, int32_t to){
    (void)to;
    int ready=0;
    for(uint32_t i=0;i<n;i++){
        fds[i].revents=0;
        if(fds[i].fd<0) continue;
        if(fds[i].events&POLLIN && ready_read(fds[i].fd)){ fds[i].revents|=POLLIN; ready++; }
        if(fds[i].events&POLLOUT && ready_write(fds[i].fd)){ fds[i].revents|=POLLOUT; ready++; }
    }
    return ready;
}
