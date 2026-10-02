#ifndef SPRING_H
#define SPRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ANIM_KIND_POSITION = 0,
    ANIM_KIND_SIZE,
    ANIM_KIND_OPACITY,
    ANIM_KIND_SCALE,
    ANIM_KIND_ROTATE
} anim_kind_t;

typedef struct {
    float stiffness;
    float damping;
    float mass;
} spring_params_t;

typedef struct {
    float pos;
    float vel;
    float target;
    spring_params_t p;
    uint8_t active;
} spring1d_t;

typedef struct {
    uint32_t window_id;
    anim_kind_t kind;
    spring1d_t x;
    spring1d_t y;
    spring1d_t w;
    spring1d_t h;
    float opacity;
    float scale;
    uint32_t start_ms;
    uint32_t duration_ms;
    uint8_t state;
} window_anim_t;

void spring1d_init(spring1d_t *s, float pos, spring_params_t p);
void spring1d_set_target(spring1d_t *s, float target);
void spring1d_step(spring1d_t *s, float dt);
bool spring1d_settled(const spring1d_t *s, float eps);
void window_anim_init(window_anim_t *a, uint32_t window_id);
void window_anim_tick(window_anim_t *a, uint32_t now_ms, float dt);

#endif
