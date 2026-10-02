#include "unix_socket.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../kernel/scheduler.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static socket_t sockets[MAX_SOCKETS];

void unix_socket_init(void){ 
    memset(sockets,0,sizeof(sockets)); 
    kprintf("[UNIX SOCKET] init\n"); 
}

static socket_t* alloc_socket(void){ 
    for(int i=0;i<MAX_SOCKETS;i++) {
        if(!sockets[i].in_use){ 
            memset(&sockets[i],0,sizeof(socket_t)); 
            sockets[i].in_use = true; 
            sockets[i].fd = i + 2000; 
            sockets[i].peer_fd = -1;
            return &sockets[i]; 
        } 
    }
    return NULL; 
}

static socket_t* get_socket(int fd){ 
    int i = fd - 2000; 
    if(i >= 0 && i < MAX_SOCKETS && sockets[i].in_use) return &sockets[i]; 
    return NULL; 
}

int sys_socket(int d,int t,int p){ 
    (void)p; 
    if (d != AF_UNIX) return -1;
    socket_t* s = alloc_socket(); 
    if(!s) return -1; 
    s->domain = d; 
    s->type = t; 
    s->state = 0;
    return s->fd; 
}

int sys_bind(int fd,const void* a,uint32_t l){ 
    (void)l; 
    socket_t* s = get_socket(fd); 
    if(!s) return -1; 
    const sockaddr_un_t *sun = (const sockaddr_un_t *)a;
    strncpy(s->path, sun->sun_path, sizeof(s->path) - 1);
    s->state = 1; /* BOUND */
    return 0; 
}

int sys_listen(int fd,int b){ 
    (void)b; 
    socket_t* s = get_socket(fd); 
    if(!s) return -1; 
    s->state = 2; /* LISTENING */
    return 0; 
}

int sys_accept(int fd,void* a,uint32_t* l){ 
    socket_t* s = get_socket(fd); 
    if(!s || s->state != 2) return -1; 
    
    while (s->backlog_count == 0) {
        process_yield(); // Wait for connections
    }
    
    int new_fd = s->backlog[0];
    for (int i = 1; i < s->backlog_count; i++) {
        s->backlog[i-1] = s->backlog[i];
    }
    s->backlog_count--;
    
    if (a && l && *l >= sizeof(sockaddr_un_t)) {
        sockaddr_un_t *sun = (sockaddr_un_t *)a;
        sun->sun_family = AF_UNIX;
        strncpy(sun->sun_path, s->path, sizeof(sun->sun_path)-1);
        *l = sizeof(sockaddr_un_t);
    }
    
    return new_fd;
}

int sys_connect(int fd,const void* a,uint32_t l){ 
    (void)l; 
    socket_t* cli = get_socket(fd); 
    if(!cli) return -1; 
    
    const sockaddr_un_t *sun = (const sockaddr_un_t *)a;
    
    /* Find listening socket */
    socket_t *srv = NULL;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].in_use && sockets[i].state == 2 && strcmp(sockets[i].path, sun->sun_path) == 0) {
            srv = &sockets[i];
            break;
        }
    }
    if (!srv) return -1;
    
    if (srv->backlog_count >= 8) return -1;
    
    /* Create server-side connected socket */
    socket_t* srv_conn = alloc_socket();
    if (!srv_conn) return -1;
    
    srv_conn->domain = cli->domain;
    srv_conn->type = cli->type;
    srv_conn->state = 3; /* CONNECTED */
    srv_conn->peer_fd = cli->fd;
    strncpy(srv_conn->path, srv->path, sizeof(srv_conn->path)-1);
    
    cli->state = 3; /* CONNECTED */
    cli->peer_fd = srv_conn->fd;
    
    /* Enqueue to listener's backlog */
    srv->backlog[srv->backlog_count++] = srv_conn->fd;
    
    return 0; 
}

int sys_socket_read(int fd,void* b,uint32_t c){ 
    socket_t* s = get_socket(fd); 
    if(!s || s->state != 3) return -1; 
    
    while (s->rx_count == 0) {
        socket_t *peer = get_socket(s->peer_fd);
        if (!peer) return 0; // Peer disconnected
        process_yield();
    }
    
    uint8_t *dst = (uint8_t *)b;
    uint32_t read_bytes = 0;
    while (read_bytes < c && s->rx_count > 0) {
        dst[read_bytes++] = s->rx_buf[s->rx_head];
        s->rx_head = (s->rx_head + 1) % SOCKET_BUF_SIZE;
        s->rx_count--;
    }
    
    return read_bytes;
}

int sys_socket_write(int fd,const void* b,uint32_t c){ 
    socket_t* s = get_socket(fd); 
    if(!s || s->state != 3) return -1; 
    
    socket_t *peer = get_socket(s->peer_fd);
    if (!peer) return -1; // Peer disconnected
    
    const uint8_t *src = (const uint8_t *)b;
    uint32_t written = 0;
    
    while (written < c) {
        while (peer->rx_count >= SOCKET_BUF_SIZE) {
            socket_t *check = get_socket(s->peer_fd);
            if (!check) return -1;
            process_yield();
        }
        
        peer->rx_buf[peer->rx_tail] = src[written++];
        peer->rx_tail = (peer->rx_tail + 1) % SOCKET_BUF_SIZE;
        peer->rx_count++;
    }
    
    return written;
}

void sys_socket_close(int fd){ 
    socket_t* s = get_socket(fd); 
    if(s) {
        if (s->peer_fd != -1) {
            socket_t *peer = get_socket(s->peer_fd);
            if (peer) {
                peer->peer_fd = -1; // Notify peer of disconnect
            }
        }
        s->in_use = false; 
    }
}
