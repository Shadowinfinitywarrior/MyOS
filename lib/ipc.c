#include "ipc.h"
#include "../kernel/pipe.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int wm_pipe_fd[2];

int ipc_get_pipe_fd(int index){
    if(index<0 || index>1) return -1;
    return wm_pipe_fd[index];
}

int ipc_init(void)
{
    if (pipe_create(wm_pipe_fd) != 0) {
        return -1;
    }
    return 0;
}

int ipc_send(int fd, const ipc_msg_t *msg)
{
    if (!msg) return -1;
    /* First write type and length as uint32_t each */
    uint32_t header[2] = {msg->type, msg->len};
    if (pipe_write(fd, header, sizeof(header)) != sizeof(header))
        return -1;
    if ((uint32_t)pipe_write(fd, msg->data, msg->len) != msg->len)
        return -1;
    return 0;
}

int ipc_recv(int fd, ipc_msg_t *msg)
{
    if (!msg) return -1;
    uint32_t header[2] = {0};
    if (pipe_read(fd, header, sizeof(header)) != sizeof(header))
        return -1;
    msg->type = header[0];
    msg->len = header[1];
    if (msg->len > sizeof(msg->data)) msg->len = sizeof(msg->data);
    if ((uint32_t)pipe_read(fd, msg->data, msg->len) != msg->len)
        return -1;
    return 0;
}
