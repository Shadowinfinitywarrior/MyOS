#ifndef IPC_H
#define IPC_H

#include "../include/types.h"
#include "process.h"

/* IPC message types */
typedef enum {
    IPC_MSG_NONE = 0,
    IPC_MSG_HELLO,
    IPC_MSG_CREATE_SURFACE,
    IPC_MSG_SURFACE_CREATED,
    IPC_MSG_ATTACH_BUFFER,
    IPC_MSG_COMMIT,
    IPC_MSG_DAMAGE,
    IPC_MSG_SET_POSITION,
    IPC_MSG_SET_SIZE,
    IPC_MSG_SET_TITLE,
    IPC_MSG_REQUEST_FOCUS,
    IPC_MSG_KEYBOARD_EVENT,
    IPC_MSG_POINTER_EVENT,
    IPC_MSG_FRAME_DONE,
    IPC_MSG_CLOSE_SURFACE,
    IPC_MSG_ERROR,
    IPC_MSG_ANIM_REQUEST,
    IPC_MSG_ANIM_CANCEL,
    IPC_MSG_GESTURE,
    IPC_MSG_SHM_CREATE,
    IPC_MSG_SHM_ATTACH,
    IPC_MSG_SHM_DETACH,
    IPC_MSG_SHM_DESTROY,
    IPC_MSG_CAP_GRANT,
    IPC_MSG_CAP_REVOKE,
} ipc_msg_type_t;

/* Maximum message payload size */
#define IPC_MAX_PAYLOAD 256

/* IPC message structure */
typedef struct ipc_msg {
    ipc_msg_type_t type;
    uint32_t       len;
    uint32_t       flags;
    uint64_t       seq;
    pid_t          sender_pid;
    uint8_t        data[IPC_MAX_PAYLOAD];
} ipc_msg_t;

/* IPC endpoint (port) */
#define IPC_MAX_PORTS 64
#define IPC_PORT_NAME_MAX 32
#define IPC_QUEUE_SIZE 16

typedef struct ipc_port {
    char              name[IPC_PORT_NAME_MAX];
    pid_t             owner_pid;
    bool              in_use;
    ipc_msg_t         queue[IPC_QUEUE_SIZE];
    int               head;
    int               tail;
    int               count;
    process_t        *waiting_process;
} ipc_port_t;

/* Capability-based security */
typedef struct ipc_capability {
    uint64_t id;
    pid_t    owner_pid;
    pid_t    target_pid;
    uint32_t port_id;
    uint32_t rights;    /* bitmask of allowed operations */
    bool     valid;
} ipc_capability_t;

#define CAP_RIGHT_SEND      (1 << 0)
#define CAP_RIGHT_RECV      (1 << 1)
#define CAP_RIGHT_SHM_MAP   (1 << 2)
#define CAP_RIGHT_SHM_CREATE (1 << 3)
#define CAP_RIGHT_SURFACE   (1 << 4)

/* IPC system initialization */
void ipc_init(void);

/* Port management */
int  ipc_port_create(const char *name);
int  ipc_port_find(const char *name);
int  ipc_port_destroy(int port_id);
int  ipc_port_send(int port_id, const ipc_msg_t *msg);
int  ipc_port_recv(int port_id, ipc_msg_t *msg, int block);

/* Capability management */
int  ipc_cap_grant(pid_t target_pid, uint32_t port_id, uint32_t rights);
int  ipc_cap_revoke(pid_t target_pid, uint32_t port_id);
int  ipc_cap_check(pid_t sender_pid, uint32_t port_id, uint32_t required_rights);

/* Shared memory integration with IPC */
int  ipc_shm_create(const char *name, uint32_t size, uint32_t *out_shmid);
int  ipc_shm_attach(int shmid, void **out_addr);
int  ipc_shm_detach(int shmid, void *addr);
int  ipc_shm_destroy(int shmid);

/* MYDP protocol integration */
int  ipc_mydp_handle_message(pid_t sender, const ipc_msg_t *msg, ipc_msg_t *reply);

/* Event bus for cross-process communication */
typedef void (*ipc_event_handler_t)(pid_t sender, const ipc_msg_t *msg);
int  ipc_event_subscribe(uint32_t event_type, ipc_event_handler_t handler);
int  ipc_event_publish(uint32_t event_type, const ipc_msg_t *msg);

#endif /* IPC_H */