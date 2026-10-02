#ifndef MYDP_PROTOCOL_H
#define MYDP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define MYDP_MAGIC 0x4D5950U
#define MYDP_VERSION 1
#define MYDP_SOCKET_PATH "/tmp/mydp"

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t length;
    uint64_t seq;
} mydp_header_t;
#pragma pack(pop)

typedef enum {
    MYDP_HELLO = 1,
    MYDP_HELLO_REPLY,
    MYDP_CREATE_SURFACE,
    MYDP_SURFACE_CREATED,
    MYDP_ATTACH_BUFFER,
    MYDP_COMMIT,
    MYDP_DAMAGE,
    MYDP_SET_POSITION,
    MYDP_SET_SIZE,
    MYDP_SET_TITLE,
    MYDP_REQUEST_FOCUS,
    MYDP_KEYBOARD_EVENT,
    MYDP_POINTER_EVENT,
    MYDP_FRAME_DONE,
    MYDP_CLOSE_SURFACE,
    MYDP_ERROR,
    MYDP_ANIM_REQUEST,
    MYDP_ANIM_CANCEL,
    MYDP_GESTURE,
} mydp_msg_type_t;

typedef enum {
    MYDP_FORMAT_ARGB8888 = 1,
} mydp_format_t;

typedef enum {
    MYDP_EBAD_SURFACE = 1,
    MYDP_EBAD_BUFFER,
    MYDP_EINVAL,
    MYDP_ENOSPC,
} mydp_error_t;

typedef struct {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint64_t phys;
    uint64_t virt;
    uint32_t size;
} mydp_buffer_t;

typedef struct {
    uint32_t surface_id;
    uint32_t buffer_id;
} mydp_attach_buffer_t;

typedef struct {
    uint32_t surface_id;
    int x, y;
    int w, h;
} mydp_damage_t;

typedef struct {
    uint32_t surface_id;
    int x, y;
} mydp_set_position_t;

typedef struct {
    uint32_t surface_id;
    uint32_t w, h;
} mydp_set_size_t;

typedef struct {
    uint32_t surface_id;
    char title[256];
} mydp_set_title_t;

typedef struct {
    uint32_t surface_id;
} mydp_request_focus_t;

typedef struct {
    uint32_t surface_id;
    uint32_t keycode;
    uint8_t state; /* 0 down, 1 up */
    uint64_t timestamp;
} mydp_keyboard_event_t;

typedef struct {
    uint32_t surface_id;
    int x;
    int y;
    uint8_t buttons;
} mydp_pointer_event_t;

typedef struct {
    uint32_t surface_id;
    uint64_t timestamp;
} mydp_frame_done_t;

typedef struct {
    uint32_t surface_id;
    uint32_t error;
    char msg[128];
} mydp_error_t_msg;

typedef struct {
    uint32_t surface_id;
    uint32_t kind; /* 0 move, 1 resize, 2 opacity */
    int32_t delta_x;
    int32_t delta_y;
    float target;
    uint32_t duration_ms;
} mydp_anim_request_t;

typedef struct {
    uint32_t surface_id;
    uint32_t gesture; /* tap, pinch, etc */
    int x, y;
} mydp_gesture_t;

/* Compositor scene graph minimal types */
typedef enum {
    COMP_NODE_SURFACE = 0,
    COMP_NODE_WINDOW  = 1,
    COMP_NODE_LAYER   = 2,
    COMP_NODE_CURSOR  = 3,
} comp_node_type_t;

typedef struct {
    int x, y, w, h;
} rect_t;

typedef struct {
    int x, y, w, h;
} comp_damage_rect_t;

#define COMP_MAX_DAMAGE_PER_NODE 64

typedef struct comp_node {
    uint32_t id;
    comp_node_type_t type;
    uint32_t surface_id;
    rect_t bounds;
    rect_t clip;
    float transform[6];
    int z_index;
    uint32_t flags;
    struct comp_node *parent;
    struct comp_node *first_child;
    struct comp_node *next_sibling;
    struct comp_node *prev_sibling;
    comp_damage_rect_t damage[COMP_MAX_DAMAGE_PER_NODE];
    int damage_count;
    bool full_damage;
} comp_node_t;

#endif /* MYDP_PROTOCOL_H */
