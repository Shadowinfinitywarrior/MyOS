#include "ipc.h"
#include "process.h"
#include "heap.h"
#include "scheduler.h"
#include "shm.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../include/mydp/protocol.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

/* Global IPC state */
static ipc_port_t ports[IPC_MAX_PORTS];
static ipc_capability_t capabilities[256];
static int capability_count = 0;

/* Event bus handlers */
#define IPC_MAX_EVENT_HANDLERS 32
static struct {
    uint32_t event_type;
    ipc_event_handler_t handler;
    bool in_use;
} event_handlers[IPC_MAX_EVENT_HANDLERS];

/* Initialize IPC system */
void ipc_init(void) {
    memset(ports, 0, sizeof(ports));
    memset(capabilities, 0, sizeof(capabilities));
    memset(event_handlers, 0, sizeof(event_handlers));
    capability_count = 0;
    kprintf("[IPC] IPC system initialized\n");
}

/* Find free port slot */
static int find_free_port(void) {
    for (int i = 0; i < IPC_MAX_PORTS; i++) {
        if (!ports[i].in_use) return i;
    }
    return -1;
}

/* Find port by name */
static int find_port_by_name(const char *name) {
    for (int i = 0; i < IPC_MAX_PORTS; i++) {
        if (ports[i].in_use && strcmp(ports[i].name, name) == 0) return i;
    }
    return -1;
}

/* Create a new IPC port */
int ipc_port_create(const char *name) {
    if (!name || strlen(name) >= IPC_PORT_NAME_MAX) return -1;
    
    int slot = find_free_port();
    if (slot < 0) return -1;
    
    if (find_port_by_name(name) >= 0) return -1;
    
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    ports[slot].in_use = true;
    strncpy(ports[slot].name, name, IPC_PORT_NAME_MAX - 1);
    ports[slot].owner_pid = proc->pid;
    ports[slot].head = 0;
    ports[slot].tail = 0;
    ports[slot].count = 0;
    ports[slot].waiting_process = NULL;
    
    /* Grant owner full rights */
    ipc_cap_grant(proc->pid, slot, CAP_RIGHT_SEND | CAP_RIGHT_RECV | CAP_RIGHT_SHM_MAP | CAP_RIGHT_SHM_CREATE | CAP_RIGHT_SURFACE);
    
    kprintf("[IPC] Created port '%s' (id=%d) for PID %d\n", name, slot, proc->pid);
    return slot;
}

/* Find port by name */
int ipc_port_find(const char *name) {
    return find_port_by_name(name);
}

/* Destroy an IPC port */
int ipc_port_destroy(int port_id) {
    if (port_id < 0 || port_id >= IPC_MAX_PORTS) return -1;
    if (!ports[port_id].in_use) return -1;
    
    process_t *proc = process_get_current();
    if (!proc || ports[port_id].owner_pid != proc->pid) return -1;
    
    /* Revoke all capabilities for this port */
    for (int i = 0; i < capability_count; i++) {
        if (capabilities[i].valid && capabilities[i].port_id == (uint32_t)port_id) {
            capabilities[i].valid = false;
        }
    }
    
    ports[port_id].in_use = false;
    kprintf("[IPC] Destroyed port %d\n", port_id);
    return 0;
}

/* Send message to port */
int ipc_port_send(int port_id, const ipc_msg_t *msg) {
    if (port_id < 0 || port_id >= IPC_MAX_PORTS) return -1;
    if (!ports[port_id].in_use) return -1;
    if (!msg) return -1;
    
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    /* Check capability */
    if (!ipc_cap_check(proc->pid, (uint32_t)port_id, CAP_RIGHT_SEND)) {
        kprintf("[IPC] PID %d denied SEND to port %d\n", proc->pid, port_id);
        return -1;
    }
    
    /* Check queue space */
    if (ports[port_id].count >= 64) return -1;
    
    /* Copy message to queue */
    ipc_msg_t *queue_msg = &ports[port_id].queue[ports[port_id].tail];
    memcpy(queue_msg, msg, sizeof(ipc_msg_t));
    queue_msg->sender_pid = proc->pid;
    
    ports[port_id].tail = (ports[port_id].tail + 1) % 64;
    ports[port_id].count++;
    
    /* Wake waiting process */
    if (ports[port_id].waiting_process) {
        process_unblock(ports[port_id].waiting_process);
        ports[port_id].waiting_process = NULL;
    }
    
    return 0;
}

/* Receive message from port */
int ipc_port_recv(int port_id, ipc_msg_t *msg, int block) {
    if (port_id < 0 || port_id >= IPC_MAX_PORTS) return -1;
    if (!ports[port_id].in_use) return -1;
    if (!msg) return -1;
    
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    /* Check capability */
    if (!ipc_cap_check(proc->pid, (uint32_t)port_id, CAP_RIGHT_RECV)) {
        kprintf("[IPC] PID %d denied RECV from port %d\n", proc->pid, port_id);
        return -1;
    }
    
    while (ports[port_id].count == 0) {
        if (!block) return -1;
        
        /* Block until message arrives */
        cli();
        proc->state = PROC_BLOCKED;
        ports[port_id].waiting_process = proc;
        scheduler_schedule();
        sti();
        
        /* Re-check port validity after wake */
        if (port_id < 0 || port_id >= IPC_MAX_PORTS || !ports[port_id].in_use) {
            return -1;
        }
    }
    
    /* Copy message from queue */
    ipc_msg_t *queue_msg = &ports[port_id].queue[ports[port_id].head];
    memcpy(msg, queue_msg, sizeof(ipc_msg_t));
    
    ports[port_id].head = (ports[port_id].head + 1) % 64;
    ports[port_id].count--;
    
    return 0;
}

/* Grant capability */
int ipc_cap_grant(pid_t target_pid, uint32_t port_id, uint32_t rights) {
    if (port_id >= IPC_MAX_PORTS) return -1;
    if (!ports[port_id].in_use) return -1;
    
    process_t *proc = process_get_current();
    if (!proc || ports[port_id].owner_pid != proc->pid) return -1;
    
    /* Check if capability already exists */
    for (int i = 0; i < capability_count; i++) {
        if (capabilities[i].valid && 
            capabilities[i].target_pid == target_pid && 
            capabilities[i].port_id == port_id) {
            capabilities[i].rights |= rights;
            return 0;
        }
    }
    
    /* Create new capability */
    if (capability_count >= 256) return -1;
    
    capabilities[capability_count].id = capability_count + 1;
    capabilities[capability_count].owner_pid = proc->pid;
    capabilities[capability_count].target_pid = target_pid;
    capabilities[capability_count].port_id = port_id;
    capabilities[capability_count].rights = rights;
    capabilities[capability_count].valid = true;
    capability_count++;
    
    kprintf("[IPC] Granted capability %llu to PID %d for port %d (rights=0x%x)\n",
            (unsigned long long)capabilities[capability_count-1].id,
            target_pid, port_id, rights);
    return 0;
}

/* Revoke capability */
int ipc_cap_revoke(pid_t target_pid, uint32_t port_id) {
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    for (int i = 0; i < capability_count; i++) {
        if (capabilities[i].valid && 
            capabilities[i].target_pid == target_pid && 
            capabilities[i].port_id == port_id &&
            capabilities[i].owner_pid == proc->pid) {
            capabilities[i].valid = false;
            kprintf("[IPC] Revoked capability %llu from PID %d for port %d\n",
                    (unsigned long long)capabilities[i].id, target_pid, port_id);
            return 0;
        }
    }
    return -1;
}

/* Check capability */
int ipc_cap_check(pid_t sender_pid, uint32_t port_id, uint32_t required_rights) {
    /* Port owner always has all rights */
    if (port_id < IPC_MAX_PORTS && ports[port_id].in_use && ports[port_id].owner_pid == sender_pid) {
        return 1;
    }
    
    for (int i = 0; i < capability_count; i++) {
        if (capabilities[i].valid && 
            capabilities[i].target_pid == sender_pid && 
            capabilities[i].port_id == port_id &&
            (capabilities[i].rights & required_rights) == required_rights) {
            return 1;
        }
    }
    return 0;
}

/* Shared memory create via IPC */
int ipc_shm_create(const char *name, uint32_t size, uint32_t *out_shmid) {
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    /* Check capability for SHM_CREATE */
    if (!ipc_cap_check(proc->pid, 0, CAP_RIGHT_SHM_CREATE)) {
        return -1;
    }
    
    int shmid = shm_create(name, size);
    if (shmid >= 0 && out_shmid) *out_shmid = (uint32_t)shmid;
    return shmid;
}

/* Shared memory attach via IPC */
int ipc_shm_attach(int shmid, void **out_addr) {
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    if (!ipc_cap_check(proc->pid, 0, CAP_RIGHT_SHM_MAP)) {
        return -1;
    }
    
    void *addr = shm_attach(shmid);
    if (addr && out_addr) *out_addr = addr;
    return addr ? 0 : -1;
}

/* Shared memory detach via IPC */
int ipc_shm_detach(int shmid, void *addr) {
    return shm_detach(shmid, addr);
}

/* Shared memory destroy via IPC */
int ipc_shm_destroy(int shmid) {
    process_t *proc = process_get_current();
    if (!proc) return -1;
    
    /* Only owner can destroy - would need to track ownership */
    return shm_destroy(shmid);
}

/* MYDP protocol message handling */
int ipc_mydp_handle_message(pid_t sender, const ipc_msg_t *msg, ipc_msg_t *reply) {
    if (!msg || !reply) return -1;
    
    process_t *sender_proc = process_get_by_pid(sender);
    if (!sender_proc) return -1;
    
    /* Initialize reply */
    reply->type = IPC_MSG_ERROR;
    reply->len = 0;
    reply->seq = msg->seq;
    reply->sender_pid = 0; /* kernel */
    
    switch (msg->type) {
        case IPC_MSG_HELLO: {
            mydp_header_t *hdr = (mydp_header_t *)msg->data;
            if (msg->len < sizeof(mydp_header_t)) break;
            if (hdr->magic != MYDP_MAGIC) break;
            
            reply->type = IPC_MSG_HELLO;
            mydp_header_t *reply_hdr = (mydp_header_t *)reply->data;
            reply_hdr->magic = MYDP_MAGIC;
            reply_hdr->version = MYDP_VERSION;
            reply_hdr->type = MYDP_HELLO_REPLY;
            reply_hdr->length = 0;
            reply_hdr->seq = hdr->seq;
            reply->len = sizeof(mydp_header_t);
            return 0;
        }
        
        case IPC_MSG_CREATE_SURFACE: {
            /* Extract surface parameters */
            if (msg->len < sizeof(uint32_t) * 2) break;
            uint32_t *params = (uint32_t *)msg->data;
            uint32_t width = params[0];
            uint32_t height = params[1];
            
            /* Create surface via Rust GUI */
            extern void *rust_gui_create_surface(int width, int height);
            void *surface = rust_gui_create_surface((int)width, (int)height);
            
            reply->type = IPC_MSG_SURFACE_CREATED;
            uint64_t *surf_ptr = (uint64_t *)reply->data;
            *surf_ptr = (uint64_t)(uintptr_t)surface;
            reply->len = sizeof(uint64_t);
            return 0;
        }
        
        case IPC_MSG_ATTACH_BUFFER: {
            if (msg->len < sizeof(mydp_attach_buffer_t)) break;
            mydp_attach_buffer_t *attach = (mydp_attach_buffer_t *)msg->data;
            
            /* Map shared memory buffer to surface */
            reply->type = IPC_MSG_ERROR;
            mydp_error_t_msg *err = (mydp_error_t_msg *)reply->data;
            err->surface_id = attach->surface_id;
            err->error = MYDP_EBAD_BUFFER;
            strcpy(err->msg, "Buffer attachment not fully implemented");
            reply->len = sizeof(mydp_error_t_msg);
            return 0;
        }
        
        case IPC_MSG_COMMIT:
        case IPC_MSG_DAMAGE:
        case IPC_MSG_SET_POSITION:
        case IPC_MSG_SET_SIZE:
        case IPC_MSG_SET_TITLE:
        case IPC_MSG_REQUEST_FOCUS:
        case IPC_MSG_KEYBOARD_EVENT:
        case IPC_MSG_POINTER_EVENT:
        case IPC_MSG_CLOSE_SURFACE: {
            /* Forward to Rust GUI or compositor */
            reply->type = msg->type;
            reply->len = 0;
            return 0;
        }
        
        case IPC_MSG_SHM_CREATE: {
            if (msg->len < SHM_NAME_MAX + sizeof(uint32_t)) break;
            char *name = (char *)msg->data;
            uint32_t *size_ptr = (uint32_t *)(msg->data + SHM_NAME_MAX);
            uint32_t size = *size_ptr;
            
            uint32_t shmid;
            int ret = ipc_shm_create(name, size, &shmid);
            if (ret >= 0) {
                reply->type = IPC_MSG_SHM_CREATE;
                uint32_t *reply_data = (uint32_t *)reply->data;
                *reply_data = shmid;
                reply->len = sizeof(uint32_t);
            }
            return ret;
        }
        
        default:
            break;
    }
    
    return -1;
}

/* Event bus subscription */
int ipc_event_subscribe(uint32_t event_type, ipc_event_handler_t handler) {
    for (int i = 0; i < IPC_MAX_EVENT_HANDLERS; i++) {
        if (!event_handlers[i].in_use) {
            event_handlers[i].event_type = event_type;
            event_handlers[i].handler = handler;
            event_handlers[i].in_use = true;
            return i;
        }
    }
    return -1;
}

/* Event bus publish */
int ipc_event_publish(uint32_t event_type, const ipc_msg_t *msg) {
    for (int i = 0; i < IPC_MAX_EVENT_HANDLERS; i++) {
        if (event_handlers[i].in_use && event_handlers[i].event_type == event_type) {
            event_handlers[i].handler(0, msg); /* sender=0 for broadcast */
        }
    }
    return 0;
}