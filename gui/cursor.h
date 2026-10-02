#ifndef GUI_CURSOR_H
#define GUI_CURSOR_H

#include "blit.h"
#include "input.h"

/* Software mouse pointer.
 *
 * QEMU hands the guest a PS/2 relative mouse, so nothing draws a cursor for us
 * and the desktop must render its own. The shape is a 12x19 arrow with a white
 * body, a dark outline, and a drop shadow, composited on top of everything
 * else each frame.
 */

#define CURSOR_W 12
#define CURSOR_H 19

typedef enum {
    CUR_ARROW = 0,
    CUR_HAND,
    CUR_IBEAM,
    CUR_RESIZE_H,
    CUR_RESIZE_V,
    CUR_RESIZE_NWSE,
    CUR_RESIZE_NESW
} cursor_shape_t;

void cursor_init(void);
void cursor_set_shape(cursor_shape_t s);
cursor_shape_t cursor_shape(void);
void cursor_set_hotspot(int x, int y);

/* Draw the cursor at its current position, with a soft shadow. */
void cursor_draw(void);
void cursor_erase(void);

#endif
