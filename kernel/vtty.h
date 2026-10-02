#ifndef KERNEL_VTTY_H
#define KERNEL_VTTY_H

#include "../include/types.h"

/* Virtual terminals.
 *
 * A vty owns a character grid (cells with fg/bg/attrs) plus an input ring.
 * Everything a process writes to its console fds lands in a vty grid; a vty
 * grid is then rendered by whichever consumer owns it -- the serial console
 * when it is the boot console, or a terminal window inside the GUI. Reading
 * from a console fd pulls from the same vty's input ring, which the GUI fills
 * from the keyboard when its window has focus.
 *
 * This is what lets the existing ring-3 shell drive a graphical window without
 * any new syscalls: sys_write/sys_read keep hitting the console device, and
 * the console device is now backed by a vty instead of straight serial I/O.
 */

#define VTTY_MAX        4
#define VTTY_COLS      100
#define VTTY_ROWS      40
#define VTTY_INPUT_CAP 256

/* Cell attributes, OR'd into vtty_cell_t.attr */
#define VTTY_ATTR_BOLD      (1 << 0)
#define VTTY_ATTR_DIM       (1 << 1)
#define VTTY_ATTR_UNDERLINE (1 << 2)
#define VTTY_ATTR_REVERSE   (1 << 3)

/* Palette indices, so a cell costs 2 bytes of colour instead of 6. */
typedef enum {
    VGA_BLACK = 0, VGA_RED, VGA_GREEN, VGA_YELLOW, VGA_BLUE,
    VGA_MAGENTA, VGA_CYAN, VGA_WHITE,
    VGA_BRIGHT_BLACK, VGA_BRIGHT_RED, VGA_BRIGHT_GREEN, VGA_BRIGHT_YELLOW,
    VGA_BRIGHT_BLUE, VGA_BRIGHT_MAGENTA, VGA_BRIGHT_CYAN, VGA_BRIGHT_WHITE,
    VGA_COLOR_COUNT
} vga_color_t;

typedef struct vtty_cell {
    /* A codepoint, not a byte. Output arrives as UTF-8 and a cell is a
     * character, so a multi-byte character has to decode to exactly one cell -
     * the MyOS banner draws itself with U+2588/U+2591, which are three bytes
     * each and would otherwise shred the character grid. */
    uint32_t  ch;
    uint8_t  fg;    /* vga_color_t */
    uint8_t  bg;
    uint8_t  attr;
} vtty_cell_t;

typedef struct vtty {
    bool          used;
    char          name[16];
    vtty_cell_t   cells[VTTY_ROWS][VTTY_COLS];
    int           cx, cy;
    uint8_t       fg, bg, attr;
    bool          cursor_visible;

    /* Incremental UTF-8 decoder, so a sequence split across two writes (the
     * write syscall moves at most 256 bytes at a time) still lands as one
     * character. u8_left counts continuation bytes still expected. */
    uint32_t      u8_pending;
    int           u8_left;

    /* Scrolling region (inclusive), used by the terminal's scrollback. */
    int           scroll_top, scroll_bottom;

    /* Input ring: bytes typed by a human (or injected) awaiting a read. */
    char          input[VTTY_INPUT_CAP];
    int           in_head, in_tail;

    /* Bumped on every mutation so a renderer can skip untouched frames. */
    uint32_t      revision;
    /* Set when the vty is attached to a window; the WM redraws on revision. */
    bool          attached;
} vtty_t;

void      vtty_init(void);
vtty_t   *vtty_alloc(const char *name);
vtty_t   *vtty_get(int id);
int       vtty_count(void);
void      vtty_free(vtty_t *v);

/* Output. vtty_putc/vtty_write interpret the same escape subset the serial
 * console understood, plus SGR colour, so ordinary programs that emit colour
 * work unchanged in a window. */
void      vtty_putc(vtty_t *v, char c);
void      vtty_write(vtty_t *v, const char *s, int len);
void      vtty_puts(vtty_t *v, const char *s);
void      vtty_clear(vtty_t *v);
void      vtty_scroll(vtty_t *v);
void      vtty_set_color(vtty_t *v, uint8_t fg, uint8_t bg);
void      vtty_set_attr(vtty_t *v, uint8_t attr);
void      vtty_move(vtty_t *v, int x, int y);

/* Input. */
void      vtty_push_char(vtty_t *v, char c);
void      vtty_push_str(vtty_t *v, const char *s);
bool      vtty_pop_char(vtty_t *v, char *out);
bool      vtty_input_empty(const vtty_t *v);
void      vtty_flush_input(vtty_t *v);

/* Grid access for renderers. */
vtty_cell_t *vtty_row(vtty_t *v, int y);
int       vtty_cols(const vtty_t *v);
int       vtty_rows(const vtty_t *v);

/* The console a process is bound to. Resolved from the process's controlling
 * vty, falling back to the boot console. */
vtty_t   *console_for_current(void);
void      console_set_boot(vtty_t *v);
vtty_t   *console_boot(void);
/* Bind the calling process to a vty (its console fds follow from here). */
void      console_bind_current(vtty_t *v);
void      console_bind_pid(int pid, vtty_t *v);

/* Render a vty to the VGA text buffer (the serial console path). */
void      vtty_render_vga(vtty_t *v);

#endif
