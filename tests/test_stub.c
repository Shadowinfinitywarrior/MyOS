#include "../gui/rect.h"
#include "../gui/surface.h"
#include "../gui/compositor.h"
#include "../gui/blit.h"
#include "../gui/input.h"
#include "../gui/anim.h"
#include "../gui/scene.h"
#include "../gui/font.h"
#include "../gui/wm2.h"
#include "../include/system.h"

extern void serial_printf(const char *fmt, ...);

void test_all_stubs(void) {
    rect_t r = {0,0,10,10};
    rect_t out;
    rect_intersect(&r,&r,&out);
    rect_union(&r,&r,&out);
    rect_contains_point(&r,1,1);
    rect_is_empty(&r);
    rect_merge_greedy(&r,1,&out,1);

    surface_t *s = surface_alloc(10,10);
    surface_free(s);
    surface_damage(s,&r);
    surface_clear_damage(s);
    surface_fill(s,0xFFFFFFFF);

    compositor_t *c = compositor_create();
    compositor_destroy(c);
    compositor_frame(c);
    compositor_request_frame(c,s);
    compositor_add_surface(c,s);
    compositor_remove_surface(c,s);

    blit_copy(0,0,0,&r,0);
    blit_blend(0,0,0,&r,0);
    blit_fill(0,&r,0,0);

    input_pump();
    input_event_t ev;
    input_poll(&ev);

    anim_t *a = anim_start(0,0,1,100,EASING_LINEAR,0);
    (void)a;
    anim_tick();
    anim_ease(EASING_LINEAR,512);

    widget_t *w = scene_create_root();
    scene_destroy(w);
    layout_run(w);
    widget_invalidate(w);

    font_t *f = font_load("/boot/fonts/DejaVuSans.ttf");
    font_draw_text(s,f,0,0,"test",0xFFFFFF,12);
    int fw,fh;
    font_measure(f,"test",12,&fw,&fh);

    wm2_window_t *win = wm2_create_window("test",0,0,100,100);
    wm2_destroy_window(win);
    wm2_run();
    wm2_update();

    serial_printf("[MOD] TEST_STUB: all stubs called\n");
}
