#ifndef SCENE_GRAPH_H
#define SCENE_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <mydp/protocol.h>

#define COMPOSITOR_MAX_SURFACES 32
#define COMP_MAX_CHILDREN 16

#define COMP_FLAG_VISIBLE (1U<<0)
#define COMP_FLAG_FOCUSED (1U<<1)
#define COMP_FLAG_OPAQUE  (1U<<2)

#define COMP_TYPE_WEIGHT_CURSOR 4
#define COMP_TYPE_WEIGHT_POPUP  3
#define COMP_TYPE_WEIGHT_PANEL  2
#define COMP_TYPE_WEIGHT_WINDOW 1
#define COMP_TYPE_WEIGHT_WALLPAPER 0

typedef struct {
    comp_damage_rect_t rects[256];
    int count;
} frame_damage_t;

typedef struct comp_scene {
    comp_node_t *root;
    comp_node_t *nodes[COMPOSITOR_MAX_SURFACES];
    int node_count;
    comp_node_t *focused;
    comp_node_t *cursor;
    int fb_width;
    int fb_height;
    uint64_t frame_number;
} comp_scene_t;

comp_scene_t *comp_scene_create(void);
void comp_scene_init(comp_scene_t *scene);
void comp_scene_destroy(comp_scene_t *scene);
comp_node_t *comp_node_create(uint32_t id, comp_node_type_t type);
void comp_node_destroy(comp_node_t *node);
void comp_node_insert(comp_node_t *parent, comp_node_t *child);
void comp_node_remove(comp_node_t *node);
int comp_node_priority(const comp_node_t *node);
void comp_transform_compose(float out[6], const float a[6], const float b[6]);
void comp_damage_add(comp_node_t *node, int x, int y, int w, int h);
void comp_damage_propagate(comp_node_t *node);
int comp_merge_node_damage(comp_node_t *node, frame_damage_t *frame);
comp_node_t *comp_hit_test(comp_node_t *root, int x, int y);
comp_node_t *comp_flatten_sorted(comp_scene_t *scene);
bool rect_empty(const rect_t *r);
rect_t rect_clip(rect_t r, rect_t clip);
bool rect_overlap_or_touch(rect_t a, rect_t b);
rect_t rect_union(rect_t a, rect_t b);
int merge_rects(comp_damage_rect_t *out, comp_damage_rect_t *in, int n);

#endif
