/* Test program to verify C↔Rust FFI bridge */
#include <stdio.h>
#include "../include/rust_gui.h"

int main(void) {
    printf("Testing Rust GUI FFI bridge...\n");

    /* Test initialization */
    int result = rust_gui_init(1024, 768);
    printf("rust_gui_init returned: %d\n", result);

    /* Test window creation */
    window_id_t wid = rust_gui_create_window("Test Window", 100, 100, 400, 300);
    printf("Created window with ID: %lu\n", wid);

    /* Test window count */
    int count = rust_gui_window_count();
    printf("Window count: %d\n", count);

    /* Test get window */
    window_t *win = rust_gui_get_window(0);
    if (win) {
        printf("Got window at index 0: id=%lu, visible=%d\n", win->id, win->visible);
    } else {
        printf("Failed to get window at index 0\n");
    }

    /* Test render */
    result = rust_gui_render_frame();
    printf("rust_gui_render_frame returned: %d\n", result);

    /* Test destroy */
    rust_gui_destroy_window(wid);
    printf("Destroyed window\n");

    printf("FFI bridge test complete\n");
    return 0;
}
