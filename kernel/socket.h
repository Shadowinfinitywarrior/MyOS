#ifndef SOCKET_H
#define SOCKET_H
#include "../include/types.h"
#define AF_UNIX 1
#define SOCK_STREAM 1
#define MAX_SOCKETS 64
#define SOCKET_BUF_SIZE 8192
typedef struct { uint16_t sun_family; char sun_path[108]; } sockaddr_un_t;
typedef struct {
    int fd; int domain; int type; int state;
    char path[108];
    uint8_t rx_buf[SOCKET_BUF_SIZE];
    uint32_t rx_head,rx_tail,rx_count;
    int peer_fd; int backlog[8]; int backlog_count;
    bool in_use;
} socket_t;
void socket_init(void);
int sys_socket(int domain,int type,int protocol);
int sys_bind(int sockfd,const void* addr,uint32_t len);
int sys_listen(int sockfd,int backlog);
int sys_accept(int sockfd,void* addr,uint32_t* len);
int sys_connect(int sockfd,const void* addr,uint32_t len);
int sys_socket_read(int sockfd,void* buf,uint32_t count);
int sys_socket_write(int sockfd,const void* buf,uint32_t count);
void sys_socket_close(int sockfd);
#endif
