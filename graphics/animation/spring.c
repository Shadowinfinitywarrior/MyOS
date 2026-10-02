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

void spring1d_init(spring1d_t *s, float pos, spring_params_t p)
{
    s->pos = pos;
    s->vel = 0.0f;
    s->target = pos;
    s->p = p;
    s->active = 0;
}

void spring1d_set_target(spring1d_t *s, float target)
{
    s->target = target;
    s->vel = 0.0f;
    s->active = 1;
}

void spring1d_step(spring1d_t *s, float dt)
{
    if (!s->active)
        return;
    if (dt <= 0.0f)
        return;
    float k = s->p.stiffness;
    float c = s->p.damping;
    float m = s->p.mass;
    if (m <= 0.0f)
        m = 1.0f;
    float force = -k * (s->pos - s->target) - c * s->vel;
    float acc = force / m;
    s->vel += acc * dt;
    s->pos += s->vel * dt;
    if ((s->pos - s->target) > -0.001f && (s->pos - s->target) < 0.001f && s->vel > -0.001f && s->vel < 0.001f) {
        s->pos = s->target;
        s->vel = 0.0f;
        s->active = 0;
    }
}

bool spring1d_settled(const spring1d_t *s, float eps)
{
    if (!s->active)
        return true;
    if ((s->pos - s->target) > -eps && (s->pos - s->target) < eps && s->vel > -eps && s->vel < eps)
        return true;
    return false;
}

void window_anim_init(window_anim_t *a, uint32_t window_id)
{
    a->window_id = window_id;
    a->kind = ANIM_KIND_POSITION;
    a->start_ms = 0;
    a->duration_ms = 0;
    a->state = 0;
    a->opacity = 1.0f;
    a->scale = 1.0f;
    spring1d_init(&a->x, 0.0f, (spring_params_t){320.0f, 36.0f, 1.0f});
    spring1d_init(&a->y, 0.0f, (spring_params_t){320.0f, 36.0f, 1.0f});
    spring1d_init(&a->w, 0.0f, (spring_params_t){480.0f, 38.0f, 1.0f});
    spring1d_init(&a->h, 0.0f, (spring_params_t){480.0f, 38.0f, 1.0f});
}

void window_anim_tick(window_anim_t *a, uint32_t now_ms, float dt)
{
    if (a->state == 0)
        return;
    if (a->state == 1) {
        a->state = 2;
        a->start_ms = now_ms;
    }
    if (a->start_ms == 0)
        a->start_ms = now_ms;
    spring1d_step(&a->x, dt);
    spring1d_step(&a->y, dt);
    spring1d_step(&a->w, dt);
    spring1d_step(&a->h, dt);
    bool sx = spring1d_settled(&a->x, 0.01f);
    bool sy = spring1d_settled(&a->y, 0.01f);
    bool sw = spring1d_settled(&a->w, 0.01f);
    bool sh = spring1d_settled(&a->h, 0.01f);
    if (sx && sy && sw && sh) {
        a->x.pos = a->x.target;
        a->y.pos = a->y.target;
        a->w.pos = a->w.target;
        a->h.pos = a->h.target;
        a->x.vel = 0.0f;
        a->y.vel = 0.0f;
        a->w.vel = 0.0f;
        a->h.vel = 0.0f;
        a->x.active = 0;
        a->y.active = 0;
        a->w.active = 0;
        a->h.active = 0;
        a->state = 0;
    }
    if (a->duration_ms > 0 && now_ms >= a->start_ms + a->duration_ms) {
        a->x.pos = a->x.target;
        a->y.pos = a->y.target;
        a->w.pos = a->w.target;
        a->h.pos = a->h.target;
        a->x.vel = 0.0f;
        a->y.vel = 0.0f;
        a->w.vel = 0.0f;
        a->h.vel = 0.0f;
        a->x.active = 0;
        a->y.active = 0;
        a->w.active = 0;
        a->h.active = 0;
        a->state = 0;
    }
}
