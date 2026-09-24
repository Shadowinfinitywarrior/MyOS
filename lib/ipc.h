#ifndef IPC_H

/*
 * Global IPC pipe file descriptors available to other modules.
 * The first descriptor (wm_pipe_fd[0]) is the read end;
 * the second (wm_pipe_fd[1]) is the write end.
 */
extern int wm_pipe_fd[2];
int ipc_get_pipe_fd(int index);
/*
 * Accessor for wm_pipe_fd array.
 * index 0 for read end, 1 for write end.
 */
int ipc_get_pipe_fd(int index);

#define IPC_H
#include "types.h"

/* Simple IPC FIFO based on kernel's pipe */
typedef struct {
    uint32_t type; /* message type */
    uint32_t len;  /* payload length */
    uint8_t data[512]; /* payload, up to 512 bytes */
} ipc_msg_t;

int ipc_init(void); /* create pipe */
int ipc_send(int fd, const ipc_msg_t *msg);
int ipc_recv(int fd, ipc_msg_t *msg);
#endif
