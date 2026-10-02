#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <mydp/protocol.h>

typedef struct {
    uint64_t ts_us;
    uint32_t device_id;
    uint8_t type;
    int32_t code;
    int32_t value;
    int32_t x;
    int32_t y;
} input_event_raw_t;

typedef struct {
    uint32_t seat_id;
    int32_t x;
    int32_t y;
    uint32_t buttons;
    uint8_t grabbed;
    uint64_t down_ts;
    int32_t down_x;
    int32_t down_y;
    float velocity_x;
    float velocity_y;
} pointer_state_t;

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_LONG_PRESS,
    GESTURE_DRAG,
    GESTURE_SWIPE,
    GESTURE_PINCH,
    GESTURE_ROTATE,
    GESTURE_PAN,
    GESTURE_SCROLL
} gesture_type_t;

typedef struct {
    gesture_type_t type;
    uint64_t ts_us;
    uint32_t window_id;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    float scale;
    float rotation;
    float velocity;
    uint32_t seq;
} gesture_event_t;

#define GESTURE_MAX_TOUCH 10

typedef struct {
    pointer_state_t ptr;
    uint8_t touch_cnt;
    int32_t touch[GESTURE_MAX_TOUCH][2];
    uint64_t last_down_ts;
    int32_t last_down_x;
    int32_t last_down_y;
    uint8_t state;
    uint32_t window_id;
    float pinch_center_x;
    float pinch_center_y;
    float pinch_prev_dist;
} gesture_ctx_t;

#define STATE_IDLE 0
#define STATE_POINTER_DOWN 1
#define STATE_DRAG 2
#define STATE_TAP 3
#define STATE_DOUBLE_TAP 4
#define STATE_LONG_PRESS 5

#define TAP_SLOP 8
#define DRAG_SLOP 6
#define TAP_MAX_MS 200
#define LONG_PRESS_MS 500
#define DBL_TAP_MS 350
#define SWIPE_MIN_VEL 600
#define VEL_ALPHA 0.3f

#define INPUT_DOWN 0
#define INPUT_UP 1
#define INPUT_MOTION 2

static void input_pump(void) { }

static void gesture_tick(gesture_ctx_t *ctx, input_event_raw_t *e) {
    if (!ctx || !e) {
        return;
    }
    uint64_t now = e->ts_us;
    switch (ctx->state) {
    case STATE_IDLE:
        if (e->type == INPUT_DOWN) {
            ctx->state = STATE_POINTER_DOWN;
            ctx->last_down_ts = e->ts_us;
            ctx->last_down_x = e->x;
            ctx->last_down_y = e->y;
            ctx->ptr.x = e->x;
            ctx->ptr.y = e->y;
            ctx->ptr.velocity_x = 0.0f;
            ctx->ptr.velocity_y = 0.0f;
            ctx->ptr.down_ts = e->ts_us;
            ctx->ptr.down_x = e->x;
            ctx->ptr.down_y = e->y;
        }
        break;
    case STATE_POINTER_DOWN:
        {
            int32_t dx = e->x - ctx->last_down_x;
            int32_t dy = e->y - ctx->last_down_y;
            int32_t d2 = dx * dx + dy * dy;
            uint64_t dur_us = now - ctx->last_down_ts;
            if (d2 > DRAG_SLOP * DRAG_SLOP) {
                ctx->state = STATE_DRAG;
                if (dur_us > 0) {
                    ctx->ptr.velocity_x = (float)dx * 1000000.0f / (float)dur_us;
                    ctx->ptr.velocity_y = (float)dy * 1000000.0f / (float)dur_us;
                }
            } else if (dur_us >= (uint64_t)LONG_PRESS_MS * 1000ULL) {
                ctx->state = STATE_LONG_PRESS;
            } else if (e->type == INPUT_UP) {
                ctx->state = STATE_IDLE;
            }
        }
        break;
    case STATE_DRAG:
        {
            int32_t dx = e->x - ctx->ptr.x;
            int32_t dy = e->y - ctx->ptr.y;
            uint64_t dt_us = now - ctx->last_down_ts;
            if (dt_us > 0) {
                float instant_vx = (float)dx * 1000000.0f / (float)dt_us;
                float instant_vy = (float)dy * 1000000.0f / (float)dt_us;
                ctx->ptr.velocity_x = VEL_ALPHA * instant_vx + (1.0f - VEL_ALPHA) * ctx->ptr.velocity_x;
                ctx->ptr.velocity_y = VEL_ALPHA * instant_vy + (1.0f - VEL_ALPHA) * ctx->ptr.velocity_y;
            }
            ctx->ptr.x = e->x;
            ctx->ptr.y = e->y;
            if (e->type == INPUT_UP) {
                ctx->state = STATE_IDLE;
            }
        }
        break;
    case STATE_LONG_PRESS:
        if (e->type == INPUT_UP) {
            ctx->state = STATE_IDLE;
        }
        break;
    default:
        ctx->state = STATE_IDLE;
        break;
    }
}

static void inputd_init(void) { }

int main(void) {
    inputd_init();
    static gesture_ctx_t ctx;
    for (;;) {
        input_pump();
        input_event_raw_t ev = {0};
        gesture_tick(&ctx, &ev);
    }
    return 0;
}
