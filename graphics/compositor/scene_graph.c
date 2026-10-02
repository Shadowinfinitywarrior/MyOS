#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mydp/protocol.h"

#define COMPOSITOR_MAX_SURFACES 32
#define COMP_FLAG_OPAQUE 1u

typedef struct {
    comp_node_t *root;
    comp_node_t *nodes[COMPOSITOR_MAX_SURFACES];
    int node_count;
    comp_node_t *focused;
    comp_node_t *cursor;
    int fb_width;
    int fb_height;
    uint64_t frame_number;
} comp_scene_t;

static comp_scene_t scene_instance;
static comp_node_t node_pool[COMPOSITOR_MAX_SURFACES];
static int node_pool_next;

static int rect_overlap_or_touch(comp_damage_rect_t a, comp_damage_rect_t b)
{
    int a_x2 = a.x + a.w;
    int a_y2 = a.y + a.h;
    int b_x2 = b.x + b.w;
    int b_y2 = b.y + b.h;
    if (a_x2 <= b.x) return 0;
    if (b_x2 <= a.x) return 0;
    if (a_y2 <= b.y) return 0;
    if (b_y2 <= a.y) return 0;
    return 1;
}

comp_scene_t *comp_scene_create(void)
{
    scene_instance.root = 0;
    scene_instance.node_count = 0;
    scene_instance.focused = 0;
    scene_instance.cursor = 0;
    scene_instance.fb_width = 0;
    scene_instance.fb_height = 0;
    scene_instance.frame_number = 0;
    for (int i = 0; i < COMPOSITOR_MAX_SURFACES; ++i) {
        scene_instance.nodes[i] = 0;
    }
    node_pool_next = 0;
    return &scene_instance;
}

void comp_scene_destroy(comp_scene_t *scene)
{
    if (!scene) return;
    scene->root = 0;
    scene->node_count = 0;
    scene->focused = 0;
    scene->cursor = 0;
    scene->fb_width = 0;
    scene->fb_height = 0;
    scene->frame_number = 0;
    for (int i = 0; i < COMPOSITOR_MAX_SURFACES; ++i) {
        scene->nodes[i] = 0;
    }
}

comp_node_t *comp_node_create(uint32_t id, comp_node_type_t type)
{
    if (node_pool_next >= COMPOSITOR_MAX_SURFACES) return 0;
    comp_node_t *n = &node_pool[node_pool_next++];
    n->id = id;
    n->type = type;
    n->surface_id = 0;
    n->bounds.x = 0;
    n->bounds.y = 0;
    n->bounds.w = 0;
    n->bounds.h = 0;
    n->clip.x = 0;
    n->clip.y = 0;
    n->clip.w = 0;
    n->clip.h = 0;
    n->transform[0] = 1.0f;
    n->transform[1] = 0.0f;
    n->transform[2] = 0.0f;
    n->transform[3] = 1.0f;
    n->transform[4] = 0.0f;
    n->transform[5] = 0.0f;
    n->z_index = 0;
    n->flags = 0;
    n->parent = 0;
    n->first_child = 0;
    n->next_sibling = 0;
    n->prev_sibling = 0;
    n->damage_count = 0;
    n->full_damage = false;
    for (int i = 0; i < COMP_MAX_DAMAGE_PER_NODE; ++i) {
        n->damage[i].x = 0;
        n->damage[i].y = 0;
        n->damage[i].w = 0;
        n->damage[i].h = 0;
    }
    return n;
}

void comp_node_destroy(comp_node_t *node)
{
    if (!node) return;
    node->id = 0;
    node->type = COMP_NODE_SURFACE;
    node->surface_id = 0;
    node->bounds.x = 0;
    node->bounds.y = 0;
    node->bounds.w = 0;
    node->bounds.h = 0;
    node->clip.x = 0;
    node->clip.y = 0;
    node->clip.w = 0;
    node->clip.h = 0;
    node->z_index = 0;
    node->flags = 0;
    node->parent = 0;
    node->first_child = 0;
    node->next_sibling = 0;
    node->prev_sibling = 0;
    node->damage_count = 0;
    node->full_damage = false;
}

void comp_damage_add(comp_node_t *n, int x, int y, int w, int h)
{
    if (!n || w <= 0 || h <= 0) return;
    int ix1 = x;
    int iy1 = y;
    int ix2 = x + w;
    int iy2 = y + h;
    int bx1 = n->bounds.x;
    int by1 = n->bounds.y;
    int bx2 = n->bounds.x + n->bounds.w;
    int by2 = n->bounds.y + n->bounds.h;
    if (ix1 < bx1) ix1 = bx1;
    if (iy1 < by1) iy1 = by1;
    if (ix2 > bx2) ix2 = bx2;
    if (iy2 > by2) iy2 = by2;
    if (ix1 >= ix2 || iy1 >= iy2) return;
    int cx1 = n->clip.x;
    int cy1 = n->clip.y;
    int cx2 = n->clip.x + n->clip.w;
    int cy2 = n->clip.y + n->clip.h;
    if (ix1 < cx1) ix1 = cx1;
    if (iy1 < cy1) iy1 = cy1;
    if (ix2 > cx2) ix2 = cx2;
    if (iy2 > cy2) iy2 = cy2;
    if (ix1 >= ix2 || iy1 >= iy2) return;
    int fw = ix2 - ix1;
    int fh = iy2 - iy1;
    if (n->full_damage) return;
    if (n->damage_count < COMP_MAX_DAMAGE_PER_NODE) {
        n->damage[n->damage_count].x = ix1;
        n->damage[n->damage_count].y = iy1;
        n->damage[n->damage_count].w = fw;
        n->damage[n->damage_count].h = fh;
        n->damage_count++;
    } else {
        n->full_damage = true;
    }
}

int merge_rects(comp_damage_rect_t *out, comp_damage_rect_t *in, int n)
{
    if (!out || !in || n <= 0) return 0;
    for (int i = 1; i < n; ++i) {
        comp_damage_rect_t key = in[i];
        int j = i - 1;
        while (j >= 0 && ((in[j].y > key.y) || (in[j].y == key.y && in[j].x > key.x))) {
            in[j + 1] = in[j];
            --j;
        }
        in[j + 1] = key;
    }
    int m = 0;
    comp_damage_rect_t cur = in[0];
    for (int i = 1; i < n; ++i) {
        comp_damage_rect_t nxt = in[i];
        if (rect_overlap_or_touch(cur, nxt)) {
            int nx1 = cur.x < nxt.x ? cur.x : nxt.x;
            int ny1 = cur.y < nxt.y ? cur.y : nxt.y;
            int cx1 = cur.x + cur.w;
            int cy1 = cur.y + cur.h;
            int nx2 = nxt.x + nxt.w;
            int ny2 = nxt.y + nxt.h;
            if (cx1 > nx2) nx2 = cx1;
            if (cy1 > ny2) ny2 = cy1;
            cur.x = nx1;
            cur.y = ny1;
            cur.w = nx2 - nx1;
            cur.h = ny2 - ny1;
        } else {
            out[m++] = cur;
            cur = nxt;
        }
    }
    out[m++] = cur;
    return m;
}

void comp_damage_propagate(comp_node_t *n)
{
    if (!n) return;
    if ((n->flags & COMP_FLAG_OPAQUE) == 0) return;
    if (!n->parent) return;
    if (n->bounds.w <= 0 || n->bounds.h <= 0) return;
    comp_damage_add(n->parent, n->bounds.x, n->bounds.y, n->bounds.w, n->bounds.h);
}

void comp_merge_node_damage(comp_node_t *n, void *frame)
{
    if (!n || !frame) return;
    typedef struct {
        comp_damage_rect_t rects[256];
        int count;
    } frame_damage_t;
    frame_damage_t *fd = (frame_damage_t *)frame;
    if (n->full_damage) {
        if (fd->count < 256) {
            fd->rects[fd->count].x = n->bounds.x;
            fd->rects[fd->count].y = n->bounds.y;
            fd->rects[fd->count].w = n->bounds.w;
            fd->rects[fd->count].h = n->bounds.h;
            fd->count++;
        }
        n->full_damage = false;
        n->damage_count = 0;
        return;
    }
    for (int i = 0; i < n->damage_count && fd->count < 256; ++i) {
        fd->rects[fd->count] = n->damage[i];
        fd->count++;
    }
    n->damage_count = 0;
}

comp_node_t *comp_hit_test(comp_node_t *root, int x, int y)
{
    if (!root) return 0;
    if (x < root->bounds.x || y < root->bounds.y) return 0;
    if (x >= root->bounds.x + root->bounds.w) return 0;
    if (y >= root->bounds.y + root->bounds.h) return 0;
    comp_node_t *child = root->first_child;
    while (child) {
        comp_node_t *hit = comp_hit_test(child, x, y);
        if (hit) return hit;
        child = child->next_sibling;
    }
    return root;
}

void comp_handle_msg(comp_scene_t *scene, uint32_t msg_type, uint32_t surface_id)
{
    if (!scene) return;
    if (msg_type == MYDP_REQUEST_FOCUS) {
        for (int i = 0; i < scene->node_count && i < COMPOSITOR_MAX_SURFACES; ++i) {
            comp_node_t *n = scene->nodes[i];
            if (n && n->surface_id == surface_id) {
                scene->focused = n;
                return;
            }
        }
    }
}
