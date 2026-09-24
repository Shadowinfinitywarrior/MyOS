# MyOS GUI Stack Public Interface

Documented per plan.md Agent 0 scaffolding.

## gui/rect.h
Types:
- rect_t { int x; int y; int w; int h; }

Functions:
- bool rect_intersect(const rect_t *a, const rect_t *b, rect_t *out) // inputs a,b, outputs intersection in out, returns true if non-empty, IRQ-safe, no alloc
- bool rect_union(const rect_t *a, const rect_t *b, rect_t *out) // inputs a,b, outputs bounding union in out, always true if valid, IRQ-safe, no alloc
- bool rect_contains_point(const rect_t *r, int x, int y) // inputs rect and point, returns containment, IRQ-safe, no alloc
- bool rect_is_empty(const rect_t *r) // inputs rect, returns true if w<=0 or h<=0, IRQ-safe, no alloc
- int rect_merge_greedy(const rect_t *in, int in_count, rect_t *out, int max_out) // inputs array, outputs merged rects, returns count, process context, no alloc

## gui/surface.h
Types:
- surface_t { uint32_t *pixels; int w; int h; int pitch; rect_t damage[64]; int damage_count; int full_damage; }

Functions:
- surface_t *surface_alloc(int w, int h) // inputs dimensions, allocates surface, returns pointer, allocates, process context
- void surface_free(surface_t *s) // inputs surface, frees resources, may be null, process context
- void surface_damage(surface_t *s, const rect_t *r) // inputs surface and rect, records damage clipped, IRQ-safe, no alloc
- void surface_clear_damage(surface_t *s) // inputs surface, resets damage list, IRQ-safe, no alloc
- void surface_fill(surface_t *s, uint32_t color) // inputs surface and color, fills entire surface and damages, process context, no alloc

## gui/compositor.h
Types:
- compositor_t { surface_t *surfaces[32]; int surface_count; surface_t *back_buffer; rect_t frame_damage[128]; int frame_damage_count; }

Functions:
- compositor_t *compositor_create(void) // allocates compositor with back buffer, returns pointer, allocates, process context
- void compositor_destroy(compositor_t *c) // frees compositor and back buffer, process context
- void compositor_frame(compositor_t *c) // merges damage, paints surfaces, flushes to framebuffer, process context, no alloc
- void compositor_request_frame(compositor_t *c, surface_t *s) // maps surface damage to frame damage, IRQ-safe, no alloc
- void compositor_add_surface(compositor_t *c, surface_t *s) // adds surface to Z list, process context, no alloc
- void compositor_remove_surface(compositor_t *c, surface_t *s) // removes surface from Z list, process context, no alloc

## gui/blit.h
Functions:
- void blit_copy(uint32_t *dst, const uint32_t *src, int src_pitch, const rect_t *r, int dst_pitch) // copies opaque rect, IRQ-safe, no alloc
- void blit_blend(uint32_t *dst, const uint32_t *src, int src_pitch, const rect_t *r, int dst_pitch) // alpha blends rect using integer math, IRQ-safe, no alloc
- void blit_fill(uint32_t *dst, const rect_t *r, int dst_pitch, uint32_t color) // fills rect with color, IRQ-safe, no alloc

## gui/input.h
Types:
- input_event_t { int type; int code; int value; }

Functions:
- void input_pump(void) // drains IRQ rings, coalesces events, process context, no alloc
- int input_poll(input_event_t *out) // returns 1 and fills out with next event or 0 if empty, process context, no alloc

## gui/anim.h
Types:
- easing_t enum { EASING_LINEAR, EASING_EASE_OUT_CUBIC, EASING_EASE_IN_OUT_QUAD }
- anim_t opaque

Functions:
- anim_t *anim_start(void *target, int from, int to, int duration_ms, easing_t easing, void (*on_done)(void *target)) // creates animation node, allocates, process context, returns handle
- void anim_tick(void) // advances all active animations, process context, no alloc
- int anim_ease(easing_t easing, int t) // returns eased value for t in [0,1024], IRQ-safe, no alloc

## gui/scene.h
Types:
- widget_type_t enum { WIDGET_BOX, WIDGET_BUTTON, WIDGET_LABEL, WIDGET_TEXT, WIDGET_SLIDER, WIDGET_SCROLL, WIDGET_IMAGE, WIDGET_CHECKBOX }
- widget_t { widget_type_t type; rect_t rect; widget_t *parent; widget_t *first_child; widget_t *last_child; widget_t *next_sibling; widget_t *prev_sibling; }

Functions:
- widget_t *scene_create_root(void) // allocates root widget, returns pointer, allocates, process context
- void scene_destroy(widget_t *root) // frees widget tree recursively, process context
- void layout_run(widget_t *root) // computes widget rects from flex properties, process context, no alloc
- void widget_invalidate(widget_t *w) // marks widget damage up to owning surface, IRQ-safe, no alloc

## gui/font.h
Types:
- glyph_t { int x; int y; int w; int h; int advance; int bearing_x; int bearing_y; }
- font_t { glyph_t glyphs[256]; }

Functions:
- font_t *font_load(const void *atlas, const void *meta) // loads atlas and meta into font, allocates, process context
- void font_draw_text(surface_t *s, font_t *font, int x, int y, const char *str, uint32_t color, int size_px) // renders SDF text to surface, damages area, process context, no alloc
- void font_measure(font_t *font, const char *str, int size_px, int *w, int *h) // computes text size in pixels, IRQ-safe, no alloc

## gui/wm2.h
Types:
- wm2_window_t { surface_t *surface; widget_t *scene_root; char title[64]; int flags; int x; int y; int w; int h; int z_order; int desktop; int anim_progress; }

Functions:
- void wm2_init(void) // initializes wm2 global state, creates compositor, allocates, process context
- wm2_window_t *wm2_create_window(const char *title, int x, int y, int w, int h) // creates window with surface and scene, allocates, process context
- void wm2_destroy_window(wm2_window_t *win) // frees window resources, process context
- void wm2_close_window(wm2_window_t *win) // starts close animation then destroys window, process context, no alloc
- void wm2_minimize(wm2_window_t *win) // minimizes window, hides surface, process context, no alloc
- void wm2_restore(wm2_window_t *win) // restores minimized window, process context, no alloc
- void wm2_switch_desktop(int desktop_id) // switches virtual desktop, updates compositor surfaces, process context, no alloc
- void wm2_run(void) // main window manager loop, blocks, process context
- void wm2_update(void) // processes one frame of WM, pumps input and ticks animations, process context, no alloc

Implementation provides window struct with surface, scene root, title, flags, z-order, basic open/close animations via anim_start with ease_out_cubic, virtual desktop switching stub, task view thumbnail stub.
