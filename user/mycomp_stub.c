#include "../include/mydp/protocol.h"
#include "../include/stdint.h"
#include "../include/stddef.h"

static void mydp_log(const char *msg)
{
    /* Minimal logging placeholder */
    (void)msg;
}

int mydp_server_init(void)
{
    mydp_log("mycomp stub initialized");
    return 0;
}

/* Simple handshake handling */
int mydp_handle_message(const mydp_header_t *hdr, const void *payload)
{
    (void)payload;
    if (!hdr) return -1;
    if (hdr->magic != MYDP_MAGIC) return -1;

    switch ((mydp_msg_type_t)hdr->type) {
        case MYDP_HELLO: {
            mydp_log("Received HELLO");
            /* Respond with HELLO_REPLY */
            break;
        }
        case MYDP_CREATE_SURFACE: {
            mydp_log("CREATE_SURFACE request");
            /* Allocate surface id, return MYDP_SURFACE_CREATED */
            break;
        }
        case MYDP_COMMIT: {
            mydp_log("COMMIT");
            /* Process damage, schedule paint */
            break;
        }
        case MYDP_FRAME_DONE: {
            mydp_log("FRAME_DONE");
            break;
        }
        default:
            mydp_log("Unknown message");
            break;
    }
    return 0;
}

/* Scene graph stub */
static comp_node_t *scene_root = NULL;

int comp_scene_init(void)
{
    scene_root = 0;
    return 0;
}

int comp_damage_add(comp_node_t *node, int x, int y, int w, int h)
{
    if (!node || node->damage_count >= COMP_MAX_DAMAGE_PER_NODE) {
        return -1;
    }
    node->damage[node->damage_count].x = x;
    node->damage[node->damage_count].y = y;
    node->damage[node->damage_count].w = w;
    node->damage[node->damage_count].h = h;
    node->damage_count++;
    return 0;
}

int main(void)
{
    mydp_server_init();
    comp_scene_init();
    /* Stub main loop */
    return 0;
}
